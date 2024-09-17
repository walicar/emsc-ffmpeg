#include <emscripten.h>
#include <emscripten/bind.h>

extern "C" {
#include <libavformat/avformat.h>
}

using namespace emscripten;

// CXX can use JS funcs
EM_JS(void, display, (const char* text), {
	var element = document.getElementById("output");
	element.textContent = UTF8ToString(text);
});

void _read(std::string url) {
 	AVFormatContext *avfc = NULL;
	avformat_open_input(&avfc, url.c_str(), NULL, NULL);
	avformat_find_stream_info(avfc, NULL);
	int fps = avfc->streams[0]->avg_frame_rate.num;
	char buf[512];
	snprintf(buf, sizeof(buf), "%s has fps of %d\n", avfc->url, fps);
	display(buf);
}

// JS can use CXX funcs
EMSCRIPTEN_BINDINGS(emsc_transcode) {
	emscripten::function("read", &_read);
};