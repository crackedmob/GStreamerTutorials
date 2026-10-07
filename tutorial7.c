#include <gst/gst.h>
#include <signal.h>
#include <string.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif



volatile sig_atomic_t terminate = 0;

void handle_sigint(int sig){
    (void)sig;
    terminate = 1;
}
int tutorial_main(int argc, char *argv[]){
    GstElement *pipeline = NULL, *audio_source, *tee, *audio_queue, *audio_convert, *audio_resample, *audio_sink;
    GstElement *video_queue, *visual, *video_convert, *video_sink;
    GstBus *bus = NULL;
    GstMessage *msg = NULL;
    GstPad *tee_audio_pad = NULL, *tee_video_pad = NULL;
    GstPad *queue_audio_pad = NULL, *queue_video_pad = NULL;
    struct sigaction sa;
    int exit_code = 0;

    // initialize GStreamer
    gst_init(&argc, &argv);


    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = handle_sigint;
    if(sigaction(SIGINT, &sa, NULL) != 0){
        g_printerr("could not install the SIGINT handler.\n");
        return 1;
    }
    // signal(SIGINT, handle_sigint);
    // audiotestsrc : produces a synthetic tone
    // wavescope : consumes an audio signal and renders a waveform as if it was (admittedly cheap) oscilloscope
    // create the elements
    audio_source = gst_element_factory_make("audiotestsrc", "audio_source");
    tee = gst_element_factory_make("tee", "tee");
    audio_queue = gst_element_factory_make("queue", "audio_queue");
    audio_convert = gst_element_factory_make("audioconvert","audio_convert");
    audio_resample = gst_element_factory_make("audioresample", "audio_resample");
    audio_sink = gst_element_factory_make("autoaudiosink","audio_sink");
    video_queue = gst_element_factory_make("queue","video_queue");
    visual = gst_element_factory_make("wavescope","visual");
    video_convert = gst_element_factory_make("videoconvert","video_convert");
    video_sink = gst_element_factory_make("autovideosink","video_sink");

    // create the empty pipeline
    pipeline = gst_pipeline_new("test-pipeline");

    if(!pipeline || !audio_source || !tee || !audio_queue || !audio_convert || !audio_resample || !audio_sink || !video_queue || !visual || !video_convert || !video_sink){
        g_printerr("not all elements could be created.\n");
        exit_code = 1;
        goto cleanup;
        //return -1;
    }


    // freq property of audiotestsrc controls the frequency of the wave, and this style and shader for wavescope make the wave continuous
    // configure elements
    g_object_set(audio_source, "freq", 215.0f, NULL);
    g_object_set(visual, "shader", 0, "style", 1, NULL);


    // this block of code adds all elements to the pipeline and then links the ones
    // that can be automatically linked
    // link all elements that can be automatically linked because they have "Always" pads
    gst_bin_add_many(GST_BIN(pipeline), audio_source, tee, audio_queue, audio_convert, audio_resample, audio_sink, video_queue, visual, video_convert, video_sink, NULL);
    if(gst_element_link_many(audio_source, tee, NULL) != TRUE || gst_element_link_many(audio_queue, audio_convert, audio_resample, audio_sink, NULL) != TRUE || gst_element_link_many(video_queue, visual, video_convert, video_sink, NULL) != TRUE){
        g_printerr("elements could not be linked.\n");
        exit_code = 1;
        goto cleanup;
        //gst_object_unref(pipeline);
        //return -1;
    }


    // to link request pads, they need to be obtained by "requesting" them to the element.
    // an element might be able to produce different kinds of request pads, so, when requesting them, the desired
    // pad template name must be provided

    // we request two pads from the tee(for the audio and the video branches) with gst_element_request_pad_simple()

    // we then obtain the pads from the downstream elements to which these request pads need to be linked.
    // these are normal Always pads, so we obtain them with gst_element_get_static_pad()

    // we finally link the pads with gst_pad_link(). this is the function that
    // gst_element_link() and gst_element_link_many() use internally

    // the sink pads we have obtained needd to be released with gst_object_unref(). the 
    // request pads will be released when we no longer need them, at the end of the program.
    // manually link the tee, which has "Request" pads
    tee_audio_pad = gst_element_request_pad_simple (tee, "src_%u");
    // g_print("obtained request pad %s for audio branch.\n", gst_pad_get_name(tee_audio_pad));
    queue_audio_pad = gst_element_get_static_pad(audio_queue, "sink");
    tee_video_pad = gst_element_request_pad_simple(tee, "src_%u");
    // g_print("obtained request pad %s for video branch.\n", gst_pad_get_name(tee_video_pad));
    queue_video_pad = gst_element_get_static_pad(video_queue, "sink");
    if(!tee_audio_pad || !tee_video_pad || !queue_audio_pad || !queue_video_pad){
        g_printerr("could not obtain all pads.\n");
        exit_code = 1;
        goto cleanup;
    }
    g_print("obtained request pad %s for audio branch.\n", GST_PAD_NAME(tee_audio_pad));
    g_print("obtained request pad %s for video branch.\n", GST_PAD_NAME(tee_video_pad));

    if(gst_pad_link(tee_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK || gst_pad_link(tee_video_pad, queue_video_pad) != GST_PAD_LINK_OK){
        g_printerr("tee could not be linked.\n");
        exit_code = 1;
        goto cleanup;
        //gst_object_unref(pipeline);
        //return -1;
    }

    // gst_object_unref(queue_audio_pad);
    // gst_object_unref(queue_video_pad);

    // start playing the pipeline
    if(gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE){
        g_printerr("unable to set the pipeline to the playing state.\n");
        exit_code = 1;
        goto cleanup;
    }

    // wait until error or eos
    bus = gst_element_get_bus(pipeline);
    while(!terminate && msg == NULL){
    msg = gst_bus_timed_pop_filtered(bus, 100 * GST_MSECOND, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    }
    if(msg != NULL){
        if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR){
            GError *err = NULL;
            gchar *debug_info = NULL;
            gst_message_parse_error(msg, &err, &debug_info);
            g_printerr("error from %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
            g_printerr("debug info: %s\n", debug_info ? debug_info : "none");
            g_clear_error(&err);
            g_free(debug_info);
            exit_code = 1;
        }
        else{
            g_print("End-Of-Stream reached.\n");
        }
    }
    else{
        g_print("interrupted, shutting down.\n");
    }

    cleanup:
        if(pipeline) gst_element_set_state(pipeline, GST_STATE_NULL);
        if(tee_audio_pad){ gst_element_release_request_pad(tee, tee_audio_pad); gst_object_unref(tee_audio_pad);}
        if(tee_video_pad){ gst_element_release_request_pad(tee, tee_video_pad); gst_object_unref(tee_video_pad);}
        if(queue_audio_pad) gst_object_unref(queue_audio_pad);
        if(queue_video_pad) gst_object_unref(queue_video_pad);
        if(msg) gst_message_unref(msg);
        if(bus) gst_object_unref(bus);
        if(pipeline) gst_object_unref(pipeline);
        return exit_code;
    // release the request pads from the tee, and unref them
    /*gst_element_release_request_pad(tee, tee_audio_pad);
    gst_element_release_request_pad(tee, tee_video_pad);
    gst_object_unref(tee_audio_pad);
    gst_object_unref(tee_video_pad);

    // free resources
    if(msg != NULL){
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);

    gst_object_unref(pipeline);
    return 0;*/
}

int main(int argc, char *argv[]){
    #if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE
        return gst_macos_main((GstMainFunc) tutorial_main, argc, argv, NULL);
    #else
        return tutorial_main(argc, argv);
    #endif
}