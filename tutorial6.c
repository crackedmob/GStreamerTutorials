#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

// functions below print the capabilities in a human-friendly format
static gboolean print_field(GQuark field, const GValue *value, gpointer pfx){
    gchar *str = gst_value_serialize(value);

    g_print("%s %15s: %s\n", (gchar *) pfx, g_quark_to_string(field), str);
    g_free(str);
    return TRUE;
}

static void print_caps(const GstCaps *caps, const gchar *pfx){
    guint i;

    g_return_if_fail(caps != NULL);

    if(gst_caps_is_any(caps)){
        g_print("%sANY\n", pfx);
        return;
    }
    if(gst_caps_is_empty(caps)){
        g_print("%sEMPTY\n", pfx);
        return;
    }

    for(i = 0; i < gst_caps_get_size(caps); i++){
        GstStructure *structure = gst_caps_get_structure(caps, i);

        g_print("%s%s\n", pfx, gst_structure_get_name(structure));
        gst_structure_foreach(structure, print_field, (gpointer) pfx);
    }
}

// prints information about a pad template, including its capabilities
static void print_pad_templates_information(GstElementFactory *factory){
    const GList *pads;
    GstStaticPadTemplate *padtemplate;

    g_print("pad template for %s:\n", gst_element_factory_get_longname(factory));
    if(!gst_element_factory_get_num_pad_templates(factory)){
        g_print(" none\n");
        return;
    }

    pads = gst_element_factory_get_static_pad_templates(factory);
    while(pads){
        padtemplate = pads->data;
        pads = g_list_next(pads);

        if(padtemplate->direction == GST_PAD_SRC)
            g_print("SRC template: '%s'\n", padtemplate->name_template);
        else if(padtemplate->direction == GST_PAD_SINK)
            g_print("SINK template: '%s'\n", padtemplate->name_template);
        else
            g_print("UNKNOWN!!! template: '%s'\n", padtemplate->name_template);

        
            if(padtemplate->presence == GST_PAD_ALWAYS)
                g_print(" Availability: Always\n");
            else if(padtemplate->presence == GST_PAD_SOMETIMES)
                g_print(" Availability: Sometimes\n");
            else if(padtemplate->presence == GST_PAD_REQUEST)
                g_print(" Availability: On request\n");
            else
                g_print(" Availability: UNKNOWN!!!\n");

            
                if(padtemplate->static_caps.string){
                    GstCaps *caps;

                    g_print(" Capabilities:\n");
                    caps = gst_static_caps_get(&padtemplate->static_caps);
                    print_caps(caps, "  ");
                    gst_caps_unref(caps);
                }

                g_print("\n");
    }
}

// shows the CURRENT capabilities of the requested pad in the given element
static void print_pad_capabilities (GstElement *element, gchar *pad_name){
    GstPad *pad = NULL;
    GstCaps *caps = NULL;


    // gst_element_get_static_pad retrieves the named pad from the given element
    // this pad is static because it is always present in the element
    // retrieve pad
    pad = gst_element_get_static_pad(element, pad_name);
    if(!pad){
        g_printerr("could not retrieve pad '%s'\n", pad_name);
        return;
    }

    // gst_pad_get_current_caps to retrieve the pad's current cpabilities, whcih can be fixed or not,
    // depending on the state of the negotiation process
    // they can even be non-existent, in which case, we call gst_pad_query_caps to retrieve the currently
    // acceptable pad capabilities
    // the currently acceptable caps will be the pad template's caps in the NULL state, 
    // but might change in the later state, as the actual hardware capabilities might be queried
    // retrieve negotiated caps(or acceptable caps if negotoation is not finished yet)
    caps = gst_pad_get_current_caps(pad);
    if(!caps){
        caps = gst_pad_query_caps(pad, NULL);
    }

    // print and free
    g_print("caps for the %s pad:\n", pad_name);
    print_caps(caps, "  ");
    gst_caps_unref(caps);
    gst_object_unref(pad);
}

int tutorial_main(int argc, char *argv[]){
    GstElement *pipeline, *source, *sink;
    GstElementFactory *source_factory, *sink_factory;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;

    // initialize gstreamer
    gst_init(&argc, &argv);


    // GstElementFactory is in charge of instantiating a particular type of element, 
    // identified by its factory name
    // create the element factories
    source_factory = gst_element_factory_find("audiotestsrc");
    // gst_element_factory_find to create a factory of type "videotestsrc", and then use it to instantiate
    // multiple "videotestsrc" elements using gst_element_factory_create.
    // gst_element_factory_make is a shortcut for gst_element_factory_find + gst_element_factory_create

    // the pad elements can already be accessed through the factories, so they can be printed as soon as
    // the factories are created
    sink_factory = gst_element_factory_find("autoaudiosink");
    if(!source_factory || !sink_factory){
        g_printerr("not all element factories could be created.\n");
        return -1;
    }

    // print information about the pad templates of these factories
    print_pad_templates_information(source_factory);
    print_pad_templates_information(sink_factory);

    // ask the factories to instantiate actual elements
    source = gst_element_factory_create(source_factory, "source");
    sink = gst_element_factory_create(sink_factory, "sink");

    // create the empty pipeline
    pipeline = gst_pipeline_new("test-pipeline");

    if(!pipeline || !source || !sink){
        g_printerr("not all elements could be created.\n");
        return -1;
    }

    // build the pipeline
    gst_bin_add_many(GST_BIN(pipeline), source, sink, NULL);
    if(gst_element_link(source, sink) != TRUE){
        g_printerr("elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    // print initial negotiated caps(in NULL state)
    g_print("in NULL state.\n");
    print_pad_capabilities(sink, "sink");

    // start playing
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if(ret == GST_STATE_CHANGE_FAILURE){
        g_printerr("unable to set the pipeline to the playing state(check the bus for error messages).\n");
    }

    // wait until error, EOS or state change
    bus = gst_element_get_bus(pipeline);
    do{
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED);

        // parse message
        if(msg != NULL){
            GError *err;
            gchar *debug_info;

            switch (GST_MESSAGE_TYPE(msg)){
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error(msg, &err, &debug_info);
                    g_printerr("error received from elements %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
                    g_printerr("debugging information: %s\n", debug_info ? debug_info : "none");
                    g_clear_error(&err);
                    g_free(debug_info);
                    terminate = TRUE;
                    break;
                case GST_MESSAGE_EOS:
                    g_print("End-Of_Stream reached.\n");
                    terminate = TRUE;
                    break;
                case GST_MESSAGE_STATE_CHANGED:
                // simply prints the current pad caps every time the state of the pipeline changes.
                // we only interested in state-changed messages from the pipelines
                if(GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)){
                    GstState old_state, new_state, pending_state;
                    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                    g_print("\nPipeline state changed from %s to %s:\n", gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
                    //print the current capabilities of the sink element
                    print_pad_capabilities(sink, "sink");
                }
                    break;
                default:
                    // we should not reach here because we only asked for ERRORs, EOS and STATE_CHANGED
                    g_printerr("unexpected message received.\n");
                    break;
            }
            gst_message_unref(msg);
        }
    }while(!terminate);

    // free resources
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    gst_object_unref(source_factory);
    gst_object_unref(sink_factory);
    return 0;
}

int main(int argc, char *argv[]){
    #if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE
        return gst_macos_main((GstMainFunc) tutorial_main, argc, argv, NULL);
    #else
        return tutorial_main(argc, argv);
    #endif
}