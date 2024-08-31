# INITIAL_MEMORY = 30 MiB
main:
	emcc app/main.cc -lembind \
	-I./dist/include -L./dist/lib \
	-lavcodec -lavformat -lavutil -lswresample \
	-s INITIAL_MEMORY=31457280 \
	-s ALLOW_MEMORY_GROWTH=1 \
	-lembind -lworkerfs.js \
    -pthread \
	-o web/assets/out.js

init:
	bash setup_dist.sh