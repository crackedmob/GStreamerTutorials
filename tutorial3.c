#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif 

// structure to contain all our information, so we can pass it to callbacks
typedef struct _CustomData{
    GstElement *pipeline;
    GstElement *source;
    GstElement *convert;
    GstElement *resample;
    GstElement *vconvert;
    GstElement *vsink;
    GstElement *sink;
} CustomData;


// handler for the pad-added signal
static void pad_added_handler(GstElement * src, GstPad * pad, CustomData * data);

int tutorial_main(int argc, char *argv[]){
    CustomData data;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;

    // initialize GStreamer
    gst_init(&argc, &argv);

    // creating elements as usual
    // create the elements
    // uridecodebin will internally instantiate(to represent) all the necessary elements (sources, demuxers and decoders)
    // to turn a URI into raw audio and/or video streams.
    // it does half the work that playbin does
    // since it contains demuxers, its source pads are not internally available and we will need to llinkto them on the fly
    data.source = gst_element_factory_make("uridecodebin", "source");
    // audioconvert is useful for converting between different audio formats, making sure that this example will work on 
    // any platform, since the format produced by the sudio decoder might not be the same that the sudio sink expects.
    data.convert = gst_element_factory_make("audioconvert", "convert");
    // audioresample is useful for converting between different audio sample rates, similarly making sure that this example
    // will work on any platform, since the sudio sample rate produced by the audio decoder
    // might not be one that the audio sink supports
    data.resample = gst_element_factory_make("audioresample", "resample");

    data.vconvert = gst_element_factory_make("videoconvert", "vconvert");

    data.vsink = gst_element_factory_make("autovideosink", "vsink");
    //autoaudiosink is the equivalent of autovideosink but for audio
    // it will render the audio stream to the audio card
    data.sink = gst_element_factory_make("autoaudiosink", "sink");

    // create an empth pipeline
    data.pipeline = gst_pipeline_new("test-pipeline");

    if(!data.pipeline || !data.source || !data.convert || !data.resample || !data.vconvert || !data.vsink || !data.sink){
        g_printerr("not all elements could be created.\n");
        return -1;
    }

    // build the pipeline
    // adding the elements to the pipeline so that we can connect them(because it is important for the elements to
    // exist in the same container to be linked with eachother.)
    gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.convert, data.resample, data.vconvert, data.vsink, data.sink, NULL);
    // here we link the converter, resample, and sink but we do not link them with the source
    // sice at this point it contains no source pads
    // we just leave this branch(converter + sink) unlinked, until later on.
    if(!gst_element_link_many(data.convert, data.resample, data.sink, NULL)){
        g_printerr("elements could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    // processes video media
    if(!gst_element_link(data.vconvert, data.vsink)){
        g_printerr("video elements could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    // set the uri to play
    // we set the URI of the file to play via a property
    g_object_set(data.source, "uri", "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm", NULL);

    // connect to the pad-added signal
    // GSignals are a crucial point in GStreamer, they allow us to be notified(by means of callbacks)
    // when something interesting has happened. Signals are identified by a name, and each [GObject] 
    // has its own signals.
    // here we are attaching to the "pad-added" signal of our source(uridecodebin element)
    // to do that we use g_signal_connect() and provide a callback function to be used pad_added_handler
    // and a data pointer. GStreamer does nothing with this, it just forwards it to the callback
    // so we can share information with it
    // in this case, we pass a pointer to the CustomData structure we built specially for this purpose
    g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);

    //start playing
    ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING); // setting the pipeline state to playing
    if(ret == GST_STATE_CHANGE_FAILURE){
        g_printerr("unable to set the pipeline to the playing state.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    // listen to the bus
    bus = gst_element_get_bus(data.pipeline);
    do{
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

        //parse message
        if(msg != NULL){
            GError *err;
            gchar *debug_info;

            switch(GST_MESSAGE_TYPE (msg)){
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error(msg, &err, &debug_info);
                    g_printerr("error received from element %s : %s\n", GST_OBJECT_NAME(msg->src), err->message);
                    g_printerr("debugging information : %s\n", debug_info ? debug_info : "none");
                    g_clear_error(&err);
                    g_free(debug_info);
                    terminate = TRUE;
                    break;
                case GST_MESSAGE_EOS:
                    g_print("End-Of-Stream reached.\n");
                    terminate = TRUE;
                    break;
                case GST_MESSAGE_STATE_CHANGED:
                // it listens to the bus messages regarding state changes and prints them on the screen 
                // every element puts message on the bus regarding its current state, so we filter them out and 
                // only listen to messages coming from the pipeline
                // we are only interested in the state-changed messages from the pipeline
                    if(GST_MESSAGE_SRC(msg) == GST_OBJECT(data.pipeline)){
                        GstState old_state, new_state, pending_state;
                        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                        g_print("pipeline state changed from %s to %s : \n", gst_element_state_get_name (old_state), gst_element_state_get_name(new_state));
                    }
                    break;
                default:
                    //we should not reach here
                    g_printerr("unexpected message received.\n");
                    break;
            }
            gst_message_unref(msg);
        }
    }while(!terminate);

    //free resources
    gst_object_unref(bus);
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
    return 0;
}


// this function will be called byt the pad-added signal
// when our source element finally has enough information to start producing data, it will
// create source pads, and trigger the  "pad-added" signall. at this point our callback will be called

// src is the GstElement which triggered the signal. in this case it can only be uridecodebin because it is the only signal we have attached
// the first parameter of a signal handler is always the object that has triggered it

// new_pad is the GstPad that has just been added to the src element
// this is usually the pad to which we want to link.

// data is the pointer we provided when attaching to the signal . here we use it to 
// pass the CustomData pointer. from CustomData we extract the converter element.
/* static void pad_added_handler(GstElement * src, GstPad * new_pad, CustomData * data){
    // then retrieve its sink pad using gst_element_get_static_pad
    // this is the pad to which we want to link new_pad
    GstPad *sink_pad = gst_element_get_static_pad(data->convert, "sink");
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    g_print("received new pad '%s' from '%s' : \n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

    //if our converter is already linked, we have nothing to do here
    // uridecodebin can create as many pads as it sees fit, and for each one, this callback will be called
    // these lines of code will prevent us from trying to link to a new pad once we are already linked
    if(gst_pad_is_linked(sink_pad)){
        g_print("we are already linked. ignoring.\n");
        goto exit;
    }

    // check the new pad's type
    // we will check the type of data this new pad is going to output, because we are only interested in 
    // pads producing audio. we have created a pipeline which deals with audio and we will not be able to link it to a pad producing video

    // gst_pad_get_current_caps retrieves the current capabilities if the pad, wrapped in a GstCaps structure.
    // all possible caps a pad can support can be queried with gst_pad_query_caps
    // a pad can offer many capabilities, and hence GstCaps can contain many GstStructure, each representing a different capability
    // the current caps on a pad will always haave aa single GstStructure and represent a single media format, or if there are no 
    // current caps yet NULL will be returned
    new_pad_caps = gst_pad_get_current_caps(new_pad);
    if(new_pad_caps == NULL){
        g_print("no caps yet on this pad\n");
        goto exit;
    }
    // here since we know that the pad we want only had one capability(audio), we retrieve the first
    // GstStructure with gst_caps_get_structure
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    // with gst_structure_get_name we recover the name of the structure, which cinatins the main description of the format(its media type , actually)
    new_pad_type = gst_structure_get_name(new_pad_struct);
    if(!g_str_has_prefix(new_pad_type, "audio/x-raw")){ // if the name is not audio/x-raw, this is not a decoded audio pad, and we are not interested in it
        g_print("it has type '%s' which is not raw audio. ignoring.\n", new_pad_type);
        goto exit;
    }

    // attempt the link
    // gst_pad_link tries to linnk two pads
    // when the pad of the right kind appears, it will be linked to the rest of the audio-processing pipeline
    // and execution will continue until ERROR or EOS
    ret = gst_pad_link(new_pad, sink_pad);
    if(GST_PAD_LINK_FAILED(ret)){
        g_print("type is '%s' but link failed.\n", new_pad_type);
    }
    else{
        g_print("link succeeded(type '%s').\n", new_pad_type);
    }

    exit: 
        // unreference the new pad's caps, if we got them
        if(new_pad_caps != NULL)
            gst_caps_unref(new_pad_caps);

        //unreference the sink pad
        gst_object_unref(sink_pad);
} */
static void pad_added_handler(GstElement *src, GstPad *new_pad, CustomData *data){
    GstPad *sink_pad = NULL;
    GstCaps *caps = gst_pad_get_current_caps(new_pad);

    if(!caps){
        g_print("no caps yet on this pad\n");
        return;
    }
    const gchar *type = gst_structure_get_name(gst_caps_get_structure(caps, 0));

    // 1. decide the target by TYPE
    if(g_str_has_prefix(type, "audio/x-raw"))
        sink_pad = gst_element_get_static_pad(data->convert, "sink");
    else if(g_str_has_prefix(type, "video/x-raw"))
        sink_pad = gst_element_get_static_pad(data->vconvert, "sink");
    else{
        g_print("ignoring type '%s'\n", type);
        goto done;
    }

    // 2. only now check whether THAT pad is taken
    if(gst_pad_is_linked(sink_pad)){
        g_print("'%s' already linked, ignoring\n", type);
        goto done;
    }

    // 3. link
    if(GST_PAD_LINK_FAILED(gst_pad_link(new_pad, sink_pad)))
        g_print("type '%s' but link failed\n", type);
    else 
        g_print("link succeeded (type '%s')\n", type);

    done:
        if(sink_pad)
            gst_object_unref(sink_pad);
        gst_caps_unref(caps);
}

int main(int argc, char *argv[]){
    #if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE
        return gst_macos_main((GstMainFunc) tutorial_main, argc, argv, NULL);
    #else
        return tutorial_main(argc, argv);
    #endif
}