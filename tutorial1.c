#include <gst/gst.h> // GStreamer header, gives access to things ike
                    /* GstElements, GstBus, GstMessage, gst_init()
                       gst_parse_launch(), gst_element_set_state(),
                        gst_element_get_bus()*/
#ifdef __APPLE__ // means if the macro has been defined, include/compile the following code
                 // __APPLE__ isn't defined, so this entire section effectively disappears during preprocessing
#include <TargetConditionals.h> // apple-provided header
                                // it defines macros that let code distinguish between
                                // different apple platforms
#endif

int tutorial_main(int argc, char *argv[]){
    GstElement *pipeline;
    GstBus *bus; // the bus is how our application receives the message from the pipeline
    GstMessage *msg; // represents the individual message received from the bus

    // initialize GStreamer
    gst_init(&argc, &argv); // initializes all the internal structures
                            // checks what plug-ins are available
                            // executes any command-line option intended for GStreame
    // if we always pass our command-line parameters argc and argv to gst_init() our application
    // will automatically benefit from the GStreamer standard command-line options


    // build the pipeline
    /* what this does is takes the uri and removes our manual work of creating a pipeline
    manually, it takes the  a textual representaion of a pipeline and turns it 
    into an actual pipeline. there is a tool completely built around it
    and that is gst-launch-1.0*/

    /*playbin is a special type of element which acts as a source and ass a sink element
      , and is a whole pipeline. internally it creates and connects all the necessary
      elements to play our media, so we do not have to worry about it. */
    pipeline = gst_parse_launch("playbin uri=https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm", NULL);


    // start playing
    // every GStreamer element has an associated state, which we can more or less
    // think of as the play/pause button in our regular DVD player
    // here the gst_element_set_state() is setting pipeline to the PLAYING state,
    // thus initiating playback.
    gst_element_set_state(pipeline, GST_STATE_PAUSED);


    // wait until error or EOS
    bus = gst_element_get_bus(pipeline); // retrieves the pipeline's bus
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS); // and this will block until we receive either an ERROR or an EOS through that bus
    // these lines will wait until an error occurs or the end of stream is found.
    //GStreamer takes care of everything. Execution will end when the media reaches its end(EOS)
    // or an error is encountered 
    // gst_bus_timed_pop_filtered(bus, timeout, message types we're interested in)

    if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR){ // here we just know that something went wrong but dunno what went wrong
        g_printerr("An error occurred! Re-run with the GST_DEBUG=*:WARN " "environment variable set for more details.\n");
    }

    // cleanup, free resources
    gst_message_unref(msg); // gst_bus_timed_pop_filtered() returned a message which need to be freed
    gst_object_unref(bus); // gst_element_get_bus() added a reference to the bus which needs to be freed
    gst_element_set_state(pipeline, GST_STATE_NULL); // setting the pipeline to the NULL state will make sure it frees any resources it has allocated
    gst_object_unref(pipeline); // unreferencing the pipeline will destroy it, and all its contents
    return 0;
}
int main(int argc, char *argv[]){ 
    // this is called conditional compilation, we can write the code and  the preprocessor selects which code gets compiled
    #if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE // compile-time conditional logic
    // if runs at runtime
    // #if is evaluated by the preprocessor before compilation
        return gst_macos_main((GstMainFunc) tutorial_main, argc, argv, NULL); // gst_macos_main() is a GStreamer helper for running the application properly in the macOS environment
    #else 
        return tutorial_main(argc, argv); // if we are not on macOS the preprocessir chooses this
    #endif
}