#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

int tutorial_main(int argc, char *argv[]){
    GstElement *pipeline, *source, *filter, *filter2, *sink;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;


    // initialize GStreamer
    gst_init(&argc, &argv);

    //create the elements
    // gst_element_factory_make(type of element we want to create, name we want to give to this particular element);
    // we have created two elements and there are no filters, so the pipeline will look like
    // source element(videotestsrc) -> sink element(autovideosink)
    /* videotestsrc : is a source element(produces data), which creates a test video pattern 
        This element is useful for debugging purposes and is not usually found in the real applications*/
    /*autovideosink : is a sink element(consumes data), which displays on a window the images it receives.
        there exist several video sinks, depending on the operating system, with a varying range of capabilities.
        autovideosink automatically selects and instantiates the best one*/
    source = gst_element_factory_make("videotestsrc", "source"); // source element
    filter = gst_element_factory_make("vertigotv", "filter"); // added a filter element (could not be linked because sink does not understand what the filter is producing)
    filter2 = gst_element_factory_make("videoconvert", "filter2"); // another element
    sink = gst_element_factory_make("autovideosink", "sink"); // sink element

    // create the empty pipeline
    // we create a pipleline with gst_pipeline_new()
    // all GStreamer elements must typically be contained inside a pipeline
    // before they can be used, because it takes care of some clocking and messaging functions.
    pipeline = gst_pipeline_new("test-pipeline"); // pipeline creation
     
    // a pipeline is a particular type of bin, which is the element used to contain other elements.
    // therefore, all methods that apply to bins also apply to pipelines.
    if(!pipeline || !source || !sink){
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    // build the pipeline
    // gst_bin_add_many() is called to add the elements to the pipeline
    // this function accepts the list of elements to be added, ending with NULL
    // gst_bin_add() : individual elements can be added using this function
    gst_bin_add_many(GST_BIN(pipeline), source, filter, filter2, sink, NULL);
    // gst_element_link(source, destination) : this is used to link those elements which we added in the pipeline
    // the parameters it takes is source, and destination and the order absolutely matters,
    // because the links must be established following the data flow.
    // the elements residing in the same bin can be linked together, therefore if we 
    // dont add an element in the pipeline(or bin) we can not  link them, which is why
    // adding the element in the bin is absolutely important, before we try to link them.
    if(gst_element_link(source, filter) != TRUE){
        g_printerr("could not link source->filter.\n");
        gst_object_unref(pipeline);
        return -1;
    }
    if(gst_element_link(filter, filter2) != TRUE){
        g_printerr("could not link filter->filter2.\n");
        gst_object_unref(pipeline);
        return -1;
    }
    if(gst_element_link(filter2, sink) != TRUE){
        g_printerr("could not link filter2->sink.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    // i can also write the above liniking piece of code
    /* if(!gst_element_link_many(source, filter, filter2, sink, NULL)){
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(pipeline); // using reference counting, so calling unref is not really deleteing the object, it is more ike relese one reference that is held to the object
    return -1;
    }*/

    // properties : GStreamer elements are all a particular kind of GObjects, which is the entity offering property facilities.
    // most GStreamer elements have customizable properties : named attributes that can be modified
    // to change the element's behavior(writable properties) or inquired to find the element's internal state(readable properties)
    // properties are read from with g_object_get()
    // properties are written to with g_object_set()
    // g_object_set() accpets a NULL-terminated list of property-name, property-value pairs, so multiple properties can be changed in one go
    // modify the source's properties
    g_object_set(source, "pattern", 0, NULL); // this changes the "pattern" property of videotestsrc
                                              // which controls the type of test video the element outputs
    // g_object_set(source, "pattern", 0,"num-buffers", 200, NULL); and it stops after 200 frames, so we will see EOS reached

    
    //start playing
    // calling gst_element_set_state(), but this time we check its return value for errors
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING); // changing states
    if(ret == GST_STATE_CHANGE_FAILURE){
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    // wait until error or EOS
    // GStreamer bus : it is the object responsible for delivering to the application the GstMessages generated by the elements,
    // in order and to the application thread.
    // the actual streaming of media is done in another thread than the application
    // the Messages can be extracted from the bus synchronously with gst_bus_timed_pop_filtered() and its siblings,
    // or asynchronously, using signals.
    // our application should always keep an eye on the bus to be notified of errors and other playback-related issues
    bus = gst_element_get_bus(pipeline);
    // gst_bus_timed_filtered() waits for execution to end and return a GstMessage which we previously ignored
    // we asked gst_bus_timed_pop_filtered() to return when GStreamer encountered either an error condition or an EOS
    // so we need to check which one happened, and print a message on screen
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    // gstMessage is a very versatile structure which can deliver virtually any kind of information.
    // fortunately, GStreamer provides a series of parsing functions for each kind of message
    //parse messages
    if(msg != NULL){
        GError *err;
        gchar *debug_info;


        // GST_MESSAGE_TYPE() helps us know if the message contains an error
        // we can use gst_message_parse_error() which returns a GLib GError error structure and  string
        // useful for debugging
        switch(GST_MESSAGE_TYPE(msg)){
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &err, &debug_info);
                // here GST_OBJECT_NAME(msg->src) names the element that complained,
                // err->message says why, and debug_info adds detail
                g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
                g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
                g_clear_error(&err);
                g_free(debug_info);
                break;
            case GST_MESSAGE_EOS:
                g_print("End-Of-Stream reached.\n");
                break;
            default:
                // we should not reach here because we only asked for ERROR or EOS
                g_printerr("Unexpected message received.\n");
                break;
        }
        gst_message_unref(msg);
    }

    // free resources
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