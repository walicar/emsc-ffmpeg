#include <emscripten.h>
#include <emscripten/bind.h>

extern "C" {
#include <libavformat/avformat.h>
}

using namespace emscripten;

// CXX can use JS funcs
EM_JS(void, call_log, (const char* text), {
	var element = document.getElementById("output");
	element.textContent = UTF8ToString(text);
});

void _read() {
 	AVFormatContext *avfc = NULL;
	avformat_open_input(&avfc, "video.mp4", NULL, NULL);
	avformat_find_stream_info(avfc, NULL);
	int fps = avfc->streams[0]->avg_frame_rate.num;
	char buf[512];
	snprintf(buf, sizeof(buf), "%s has fps of %d\n", avfc->url, fps);
	call_log(buf);
}

// JS can use CXX funcs
EMSCRIPTEN_BINDINGS(emsc_transcode) {
	emscripten::function("read", &_read);
};