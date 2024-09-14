main:
	emcc app/main.cc \
	-I./dist/include -L./dist/lib \
	-lavformat -lavutil -lavcodec -lswresample \
	-lembind \
	-o web/assets/out.js

init:
	bash scripts/setup_dist.sh