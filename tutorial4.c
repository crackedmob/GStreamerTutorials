#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif


// structure to contain all our information, so we can pass it around to other functions
typedef struct _CustomData{
    GstElement *playbin; // our one and only element
    gboolean playing; // are we in PLAYING state?
    gboolean terminate; // shoul we terminate execution
    gboolean seek_enabled; // is seeking enabled for this media?
    gboolean seek_done; // have we performed the seek already?
    gint64 duration; // how long does this media last, in nanoseconds
} CustomData;

// forward definition of the message processing function
static void handle_message(CustomData *data, GstMessage *msg);

int tutorial_main(int argc, char *argv[]){
    CustomData data;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;

    data.playing = FALSE;
    data.terminate = FALSE;
    data.seek_enabled = FALSE;
    data.seek_done = FALSE;
    data.duration = GST_CLOCK_TIME_NONE;

    // initialize GStreamer
    gst_init(&argc, &argv);


    // playbin is in itself a pipeline, and in this case the only element in the pipeline
    // so we directly use the playbin element
    // create the elements
    data.playbin = gst_element_factory_make("playbin", "playbin");

    if(!data.playbin){
        g_printerr("not all elements could be created.\n");
        return -1;
    }

    // the URI of the clip is goven to the playbin via the URI property and the pipeline is set to the playing state
    // set the URI to play
    g_object_set(data.playbin, "uri", "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm", NULL);

    // start playing
    ret = gst_element_set_state(data.playbin, GST_STATE_PLAYING);
    if(ret == GST_STATE_CHANGE_FAILURE){
        g_printerr("unable to set the pipeline to the playing state.\n");
        gst_object_unref(data.playbin);
        return -1;
    }

    // listen to the bus
    bus = gst_element_get_bus(data.playbin);
    do{// previously, we did not provide a timeout to gst_bus_timed_pop_filtered(),
        // meaning that it didn't return until a message was received.
        // now we have provided a timeout of 100 ms, so, if no message is received during 
        // tenth of a second, the function will return NULL

        // we are gonna use this logic to update our "UI"
        // the desired timeout must be secified as a GstClockTime, hence, in nanoseconds.
        // numbers expressing different time units then, should be multiplied by macros like
        // GST_SECOND or GST_MSECOND
        msg = gst_bus_timed_pop_filtered(bus, 100 * GST_MSECOND, GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_DURATION);

        // parse message
        if(msg != NULL){
            handle_message(&data, msg);
        }
        else{// if the pipeline is in PLAYING state, it is time to refresh the screen.
            // we don't want to do anythng if we are not in PLAYING state, because most queries would fail.
            // we got no message this means the timeout expired
            if(data.playing){
                gint64 current = -1;

                // gst_element_query_position hides the management of the query object and directly provides us withthe result
                // query the current position of the stream
                if(!gst_element_query_position(data.playbin, GST_FORMAT_TIME, &current)){
                    g_printerr("could not query current position.\n");
                }

                // if we did not know it yet, query the stream duration
                if(!GST_CLOCK_TIME_IS_VALID(data.duration)){
                    // gst_element_query_duration is a helper function to know the length of the stream
                    if(!gst_element_query_duration(data.playbin, GST_FORMAT_TIME, &data.duration)){
                        g_printerr("could not query current duration.\n");
                    }
                }

                // the usage of GST_TIME_FORMAT and GST_TIME_ARGS macros to provide a user-friendly
                // representation of GStreamer times
                // print current position and total duration
                g_print("position %" GST_TIME_FORMAT "/ %" GST_TIME_FORMAT "\r", GST_TIME_ARGS (current), GST_TIME_ARGS(data.duration));


                // performing seek by simply calling gst_elementz-seek_simple() on the pipeline
                // GST_FORMAT_TIME indicates that we are specifying the destination in time units
                // other seek formats use different units
                /*GstSeekFlags
                1. GST_SEEK_FLAG_FLUSH: this discards all data currently in thepipeline before doing the seek.
                    might pause a bit while the pipeline is refilled and the new data starts to show up, but generally increases
                    the "responsiveness" of the application. if this flag is not provided, "stale" data might be shown for a while
                    until the new position appears at the end of the pipeline
                    
                2. GST_SEEK_FLAG_KEY_UNIT: with most encoded video streams, seeking to arbitrary positions is not possible but only
                    to certain frames called Key Frames. when this flag is used, the seek will actually move to the closest key frame and start
                    producing data straight away. if this flag is not used, the pipeline will move internally to the closest key frame
                    (no ther alternative) but the data will not be shown until it reaches the requested position. this last alternative is more 
                    accurate , but might take longer
                
                3. GST_SEEK_FLAG_ACCURATE: some media clips do not provide enough indexing information, meaning that seeking to arbitrary positions is 
                    time-consuming. in these cases, GStreamer usually estimates the position to seek to, and usually works just fine.
                    if this precision is not good enough for our case(we seek not going to the exact time we asked for), then provide this flag.
                    it might take longer to calculate the seeking position(very long, on some files)
                    */
                // we provide the position to seek to. since we asked for GST_FORMAT_TIME, the value must be in nanoseconds
                // so we express the time in seconds, for simplicity, and then multiply by GST_SECOND
                // if seeking is enabled, we have not done it yet, and the time is right, seek
                if(data.seek_enabled && !data.seek_done && current > 10 * GST_SECOND){
                    g_print("\nreached 10s, performing seek \n");
                    gst_element_seek_simple(data.playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 30 * GST_SECOND);
                    data.seek_done = TRUE;
                }
            }
        }
    } while(!data.terminate);

    // free resources
    gst_object_unref(bus);
    gst_element_set_state(data.playbin, GST_STATE_NULL);
    gst_object_unref(data.playbin);
    return 0;
}
// handle_message function processes all messages received through the pipeline's bus
static void handle_message(CustomData *data, GstMessage *msg){
    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE(msg)){
        case GST_MESSAGE_ERROR:
            g_printerr("error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
            g_printerr("debugging information: %s\n", debug_info ? debug_info : "none");
            g_clear_error(&err);
            g_free(debug_info);
            data->terminate = TRUE;
            break;
        case GST_MESSAGE_EOS:
            g_print("\nEnd-Of-Stream reached.\n");
            data->terminate = TRUE;
            break;
        // this message is posted on the bus whenever the duration of the stream changes
        // here we simply mark the current duration as invalid, so it gets re-queried later
        case GST_MESSAGE_DURATION:
            // the duration has changed, mark the current one as invalid
            data->duration = GST_CLOCK_TIME_NONE;
            break;
        // seek and time queries generally only get a valid reply when in the PAUSED or PLAYING state
        // since all elements have had a chance to receive information and configure themselves.
        // here, we use the playing variable to keep track of whether the pipeline is in PLAYING state
        // if we have just entered a PLAYING state, we do our first query. we ask the pipeline if seeking is allowed on this stream
        case GST_MESSAGE_STATE_CHANGED:{
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
            if(GST_MESSAGE_SRC(msg) == GST_OBJECT(data->playbin)){
                g_print("pipeline state changed from %s to %s:\n", gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));

                // remember whether we are in the PLAYING state or not
                data->playing = (new_state ==  GST_STATE_PLAYING);

                // gst_query_new_seeking creates a new query object of the seeking type, with GST_FORMAT_TIME format
                // indicates that we are interested in seeking by specifying the new time to which we want to move
                // we could also ask for GST_FORMAT_BYTES, and then seek to a particular byte position inside the source file
                // but this is normally less useful

                // this query object is then passed to the pipeline with gst_element_query. the result is stored in the same query and 
                // can be easily retrieved with gst_query_parse_seeking
                // it extracts a boolena indicating if the seeking is allowed, and the range in which seeking is possible
                if(data->playing){
                    // we just moved to PLAYING. check if seeking is possible
                    GstQuery *query;
                    gint64 start, end;
                    query = gst_query_new_seeking(GST_FORMAT_TIME);
                    if(gst_element_query(data->playbin, query)){
                        gst_query_parse_seeking(query, NULL, &data->seek_enabled, &start, &end);
                        if(data->seek_enabled){
                            g_print("seeking is ENABLED from %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(start), GST_TIME_ARGS(end));
                        } else{
                            g_print("seeking is DISABLED for this stream.\n");
                        }
                    } else{
                        g_printerr("seeking query failed.");
                    }
                    gst_query_unref(query);
                }
            }
        }
            break;
        default:
        // we should not reach here
        g_printerr("unexpected message received.\n");
        break;
    }
    gst_message_unref(msg);
}

int main(int argc, char *argv[]){
    #if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE
        return gst_macos_main((GstMainFunc) tutorial_main, argc, argv, NULL);
    #else
        return tutorial_main(argc, argv);
    #endif
}