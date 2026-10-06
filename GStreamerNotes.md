TUTORIAL 1

Gstreamer is a framework designed to handle the multimedia flows. media travels from the source elements (the producers), down to the sink elements (the consumers), passing through a series of intermediate elements performing all kinds of tasks. The set of all interconnected elements is called a pipeline.

             GStreamer
                 │
                 ▼
        ┌─────────────────┐
        │     playbin     │
        │                 │
URL ──► │ internally      │
        │ builds pipeline │
        └────────┬────────┘
                 │
          ┌──────┴──────┐
          ▼             ▼
       Video           Audio
          │             │
          ▼             ▼
       Display       Speakers
important part is we dont manually create those components yet.
we just give a media URI to the GStreamer and playbin figures out the rest, playbin is a special element that internally creates and connects the elements needed for playback.


gst_init(): it initializes GStreamer, checks available plugins, and processes GStreamer command-line options.
ahy args and argv? because int main(int argc, int *argv[]) contains the command-line arguments. passing them into gst_init(&argc, &argv) allows GStreamer to process its own command-line options

Your program starts
       │
       ▼
  gst_init()
       │
       ├── initialize GStreamer internals
       ├── discover/prepare plugins
       └── process GStreamer CLI options
       │
       ▼
Now use GStreamer

we usually build the pipeline by manually assembling the individual elements, but when the pipeline is easy enough, and we do not need any advamced features we can take the shortcut : gst_parse_launch().

normally GStreamer allows us to create/construct a pipeline manually, but that is a lot of work/code. Therefore, GStreamer provides us gst_parse_launch() and it takes a textual representation/description of the pipeline, and it constructs the corresponding GStreamer objects for us

gst_parse_launch :takes a textual representation of a pipeline and turns it into an actual pipeline. this helps us a lot as we dont have to build everuthing manually everytime we send or pass a multimedia URI, we just send the URI and creates a representation of it for us as needed and reduces our manual work. there is a tool built completely around it and that is gst-launch-1.0.

playbin
okay so the main question now is what kind of pipeline we are asking the gst_parse_launch to build for us. and that is whwre the playbin comes in, we are building a pipeline composed of a single element called playbin.

playbin is a special element which acts as a source and as a sink, and is a whole pipeline. internally it creates and connects all the necessary elements to play our media, so we do not have to worry about it.

playbin is not just a normal video source, it is a high-level playback element.

we cannot use playbin all the time, because playbin is a high-level automatic playback element. It is convenient when i just need media playback, but if i need fine-grained(granularity) control over individual elements, cpas, linking, buffering, processing, or a custom media path, we'd construct the pipeline manually.

now GStreamer pipeline have states
NULL->READY->PAUSED->PLAYING
NULL: no resources/ completely uninitialized
READY: resources acquired, but not yet processing media
PAUSED: prepared to process, but not actively running
PLAYING: actively processing/playing

gst_element_set_state(pipelone, GST_STATE_PLAYING) this means we are setting the pipelones state to PLAYING(start playback/ actively process the stream), playback won't begin until the pipeline is put into PLAYING

bus = gst_element_get_bus(pipeline);
it is asking for the bus of the pipeline
msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

GST_MESSAGE_ERROR : something went wrong
GST_MESSAGE_EOS : normal completion (the video simply finished normally)

EOS indicates that the stream has ended normally; ERROR indicates that something went wrong during processing

gst_bus_timed_pop_filtered
        │
        ├── bus
        ├── timeout
        └── message types we're interested in
what we are saying here is wait until the bus receives either an error or EOS message. this call is describes as blocking until one of those messages arrive
BUS: the bus is the mechanism through which messages from GStreamer objects are delivered to the application. bus is a general application-facing message channel.

                 Pipeline
                    │
              posts message
                    ↓
                  GstBus
                    │
                    ↓
              Application

GST_CLOCK_TIME_NONE
means wait indefinitely
Start playback
     │
     ▼
Wait...
     │
     ├──── ERROR ────► exit
     │
     └──── EOS ──────► exit

the cleanup or freeing resources matters as much. because the GStreamer uses reference counting for its objects
we dont just create objects and forget about them. so once we are done working and getting the output we wanted , we free the memory or delete the references. 

                    YOUR C PROGRAM
                          │
                          │ gst_init()
                          ▼
                   GStreamer initialized
                          │
                          │ gst_parse_launch()
                          ▼
                    ┌─────────────┐
                    │   playbin   │
                    │             │
       URI ───────► │ internally  │
                    │ builds the  │
                    │ playback    │
                    │ pipeline    │
                    └──────┬──────┘
                           │
                           │ PLAYING
                           ▼
                    Media processing
                           │
                           │
                           ▼
                        GstBus
                           │
                    ┌──────┴──────┐
                    │             │
                  ERROR          EOS
                    │             │
                    └──────┬──────┘
                           ▼
                        cleanup
                           │
                           ▼
                         NULL

GstBuffer carries the actual media data being processed, together with associated metadata/timestamps.

        GstBuffer
           │
           ▼
┌──────────────────┐
│   videoconvert   │
└────────┬─────────┘
         │
      GstBuffer
         │
         ▼
┌──────────────────┐
│   videosink      │
└──────────────────┘
and pads are what allow elements to connect and exchange data


ELEMENT
   │
   │ pads
   ▼
GstBuffer(s)
   │
   ▼
ELEMENT

current model:
                 GStreamer
                     │
             ┌───────┴────────┐
             │                │
          Elements         GstBus
             │                │
             ▼                ▼
        Pipeline          Messages
             │
             ▼
          PLAYING
             │
             ▼
       media processing

videotestsrc ! videoconvert ! fakesink
videotestsrc : is a source element that generates test video frames (it generates synthetic video)
videoconvert : converts video frames between different raw video formats so that downstream elements can accept them
fakesink : is a sink element that consumes/discards the incoming data without producing an actual output

gst-launch-1.0 videotestsrc ! videoconvert ! fakesink
Generate test frames
       ↓
Convert them if necessary
       ↓
Consume them and throw them away

! : connects/llinks the source pad(s) of A to the appropriate sink pad(s) of B
videotestsrc ! videoconvert ! fakesink
      │               │             │
      └───────────────┴─────────────┘
                links

! is not itself a GStreamer runtime object. 
Its pipeline-description syntax understood by gst-launch-1.0/ gst_patse_launch()
TUTORIAL 2
