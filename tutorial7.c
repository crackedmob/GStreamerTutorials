#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

int tutorial_main(int argc, char *argv[]){
    GstElement *pipeline, *audio_source, *tee, *audio_queue, *audio_convert, *audio_resample, *audio_sink;
    GstElement *video_queue, *visual, *video_convert, *video_sink;
    GstBus *bus;
    GstMessage *msg;
    GstPad *tee_audio_pad, *tee_video_pad;
    GstPad *queue_audio_pad, *queue_video_pad;


    // initialize GStreamer
    gst_init(&argc, &argv);


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
        return -1;
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
        gst_object_unref(pipeline);
        return -1;
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
    g_print("obtained request pad %s for audio branch.\n", gst_pad_get_name(tee_audio_pad));
    queue_audio_pad = gst_element_get_static_pad(audio_queue, "sink");
    tee_video_pad = gst_element_request_pad_simple(tee, "src_%u");
    g_print("obtained request pad %s for video branch.\n", gst_pad_get_name(tee_video_pad));
    queue_video_pad = gst_element_get_static_pad(video_queue, "sink");
    if(gst_pad_link(tee_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK){
        g_printerr("tee could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    gst_object_unref(queue_audio_pad);
    gst_object_unref(queue_video_pad);

    // start playing the pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // wait until error or eos
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    // release the request pads from the tee, and unref them
    gst_element_release_request_pad(tee, tee_audio_pad);
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
    return 0;
}

int main(int argc, char *argv[]){
    #if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE
        return gst_macos_main((GstMainFunc) tutorial_main, argc, argv, NULL);
    #else
        return tutorial_main(argc, argv);
    #endif
}