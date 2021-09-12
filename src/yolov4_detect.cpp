#include "yolov4_detect.hpp"

void
addDisplayMeta(gpointer batch_meta_data, gpointer frame_meta_data, ObjCount cnt) {

    NvDsBatchMeta *batch_meta = (NvDsBatchMeta *)batch_meta_data;
    NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)frame_meta_data;

    // To access the data that will be used to draw
    NvDsDisplayMeta *display_meta = NULL;
    NvOSD_TextParams *txt_params = NULL;
    NvOSD_LineParams *line_params = NULL;

    int offset = 0;
    display_meta = nvds_acquire_display_meta_from_pool(batch_meta);
    txt_params = display_meta->text_params;
    line_params = display_meta->line_params;
    display_meta->num_labels = 1;
    
    txt_params->display_text = (char *)g_malloc0(MAX_DISPLAY_LEN);

    offset = snprintf(txt_params->display_text, MAX_DISPLAY_LEN, 
    "Person Count  %d\nCar Count       %d",
    cnt.PersonCount, cnt.CarCount);

    /* Now set the offsets where the string should appear */
    txt_params->x_offset = 10;
    txt_params->y_offset = 12;

    /* Font , font-color and font-size */
    txt_params->font_params.font_name = (char *)"Serif";
    txt_params->font_params.font_size = 12;
    txt_params->font_params.font_color.red = 1.0;
    txt_params->font_params.font_color.green = 1.0;
    txt_params->font_params.font_color.blue = 1.0;
    txt_params->font_params.font_color.alpha = 1.0;

    /* Text background color */
    txt_params->set_bg_clr = 1;
    txt_params->text_bg_clr.red = 0.0;
    txt_params->text_bg_clr.green = 0.0;
    txt_params->text_bg_clr.blue = 0.0;
    txt_params->text_bg_clr.alpha = 1.0;

    nvds_add_display_meta_to_frame(frame_meta, display_meta);
}

static GstPadProbeReturn
osd_sink_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info,
    gpointer u_data)
{
    GstBuffer *buf = (GstBuffer *) info->data;
    NvDsBatchMeta *batch_meta  = NULL;
    NvDsObjectMeta *obj_meta = NULL;
    NvDsDisplayMeta *display_meta = NULL;
    NvDsFrameMeta *frame_meta = NULL;
    NvDsMetaList * l_frame = NULL;
    NvDsMetaList * l_obj = NULL;

    batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    if (!batch_meta) {
        std::cout<<"no batch meta"<<std::endl;
        return GST_PAD_PROBE_OK;
    }

    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
    l_frame = l_frame->next) {
        ObjCount cnt;
        frame_meta = (NvDsFrameMeta *) (l_frame->data);

        if (frame_meta == NULL) {
            // Ignore Null frame meta.
            continue;
        }

        for (l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next) {
            obj_meta = (NvDsObjectMeta *) (l_obj->data);
            if (obj_meta == NULL) {
                // Ignore Null object.
                continue;
            }
            guint class_index = obj_meta->class_id;

            if (obj_meta->class_id == cnt.PersonID) {
                cnt.PersonCount++;
            }
            if (obj_meta->class_id == cnt.CarID) {
                cnt.CarCount++;
            }
        }
        // Add Information on to the stream
        addDisplayMeta(batch_meta, frame_meta, cnt);
    }
    return GST_PAD_PROBE_OK;
}

int
configure_element_properties(GstElement *source,
GstElement *streammux, GstElement *pgie_detector, char *video_path) {
    g_object_set(G_OBJECT(source), "location", video_path, NULL);
    g_object_set(G_OBJECT(streammux), "width", MUXER_OUTPUT_WIDTH,
            "height", MUXER_OUTPUT_HEIGHT, "batch-size", 1,
            "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC, NULL);

    // Set all important properties of pgie_detector
    g_object_set(G_OBJECT(pgie_detector),
                "config-file-path", PGIE_YOLO_DETECTOR_CONFIG_FILE_PATH, NULL);

    // Check if Engine Exists
    if (boost::filesystem::exists(boost::filesystem::path(
        PGIE_YOLO_ENGINE_PATH))) {
        g_object_set(G_OBJECT(pgie_detector),
        "model-engine-file", PGIE_YOLO_ENGINE_PATH, NULL);
    }
    else {
        std::cout<<str(boost::format("YOLO Engine not found."))<<std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static gboolean
bus_call (GstBus * bus, GstMessage * msg, gpointer data){
    GMainLoop *loop = (GMainLoop *) data;
    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS:{
            g_print ("End of stream\n");
            g_main_loop_quit (loop);
            break;
        }
        case GST_MESSAGE_ERROR:{
            gchar *debug;
            GError *error;
            gst_message_parse_error (msg, &error, &debug);
            g_printerr ("ERROR from element %s: %s\n",
                GST_OBJECT_NAME (msg->src), error->message);
            if (debug)
                g_printerr ("Error details: %s\n", debug);
            g_free (debug);
            g_error_free (error);
            g_main_loop_quit (loop);
            break;
        }
        default:
            break;
    }
    return TRUE;
}

int
main (int argc, char *argv[])
{
    // DETECTOR::Dexter dexter;
    GMainLoop *loop = NULL;
    GstElement *pipeline = NULL, *source = NULL, *h264parser = NULL,
        *decoder = NULL, *streammux = NULL, *sink = NULL, *pgie = NULL, *nvvidconv = NULL,
        *nvosd = NULL;

    GstElement *transform = NULL;
    GstBus *bus = NULL;
    guint bus_watch_id;
    GstPad *osd_sink_pad = NULL;

    /* Standard GStreamer initialization */
    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);
    pipeline = gst_pipeline_new ("dstest1-pipeline");
    source = gst_element_factory_make ("filesrc", "file-source");
    h264parser = gst_element_factory_make ("h264parse", "h264-parser");
    decoder = gst_element_factory_make ("nvv4l2decoder", "nvv4l2-decoder");
    streammux = gst_element_factory_make ("nvstreammux", "stream-muxer");
    if (!pipeline || !streammux) {
        g_printerr ("One element could not be created. Exiting.\n");
        return -1;
    }

    pgie = gst_element_factory_make ("nvinfer", "primary-nvinference-engine");
    nvvidconv = gst_element_factory_make ("nvvideoconvert", "nvvideo-converter");
    nvosd = gst_element_factory_make ("nvdsosd", "nv-onscreendisplay");
    // to stream the output
    sink = gst_element_factory_make ("nveglglessink", "nvvideo-renderer");
    // to save the output as a file instead of streaming | needs bug fix.
    // sink = gst_element_factory_make("filesink", "save-file");
    // g_object_set(G_OBJECT(sink), "location", argv[2], NULL);

    if (!source || !h264parser || !decoder || !pgie
        || !nvvidconv || !nvosd || !sink) {
        g_printerr ("One element could not be created. Exiting.\n");
        return -1;
    }

    int fail_safe = configure_element_properties(source, streammux, pgie, argv[1]);
    if(fail_safe == -1) {
        return -1;
    }

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref (bus);

    gst_bin_add_many (GST_BIN (pipeline),
        source, h264parser, decoder, streammux, pgie,
        nvvidconv, nvosd, sink, NULL);

    // --------------
    GstPad *sinkpad, *srcpad;
    gchar pad_name_sink[16] = "sink_0";
    gchar pad_name_src[16] = "src";

    sinkpad = gst_element_get_request_pad (streammux, pad_name_sink);
    if (!sinkpad) {
        g_printerr ("Streammux request sink pad failed. Exiting.\n");
        return -1;
    }

    srcpad = gst_element_get_static_pad (decoder, pad_name_src);
    if (!srcpad) {
        g_printerr ("Decoder request src pad failed. Exiting.\n");
        return -1;
    }

    if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
        g_printerr ("Failed to link decoder to stream muxer. Exiting.\n");
        return -1;
    }

    gst_object_unref (sinkpad);
    gst_object_unref (srcpad);
    // ---------------------

    if (!gst_element_link_many (source, h264parser, decoder, NULL)) {
        g_printerr ("Elements could not be linked: 1. Exiting.\n");
        return -1;
    }

    if (!gst_element_link_many (streammux, pgie,
        nvvidconv, nvosd, sink, NULL)) {
        g_printerr ("Elements could not be linked: 2. Exiting.\n");
        return -1;
    }

    osd_sink_pad = gst_element_get_static_pad (nvosd, "sink");
    if (!osd_sink_pad)
        g_print ("Unable to get sink pad\n");
    else
        gst_pad_add_probe(osd_sink_pad, GST_PAD_PROBE_TYPE_BUFFER,
            osd_sink_pad_buffer_probe, NULL, NULL);
    gst_object_unref (osd_sink_pad);

    /* Set the pipeline to "playing" state */
    g_print ("Now playing: %s\n", argv[1]);
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* Wait till pipeline encounters an error or EOS */
    g_print ("Running...\n");
    g_main_loop_run (loop);

    /* Out of the main loop, clean up nicely */
    g_print ("stopping playback\n");
    gst_element_set_state (pipeline, GST_STATE_NULL);
    g_print ("Deleting pipeline\n");
    gst_object_unref (GST_OBJECT (pipeline));
    g_source_remove (bus_watch_id);
    g_main_loop_unref (loop);
    return 0;
}

