main:
	emcc app/main.cc \
	-I./dist/include -L./dist/lib \
	-lavformat -lavutil -lavcodec -lswresample \
	-lembind \
	-s INITIAL_MEMORY=26214400 \
	-s ALLOW_MEMORY_GROWTH=1 \
	--embed-file app/video.mp4@video.mp4 \
	-o web/assets/out.js

init:
	bash scripts/setup_dist.sh