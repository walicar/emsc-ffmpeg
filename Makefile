# INITIAL_MEMORY = 35 MiB
main:
	emcc app/main.cc -lembind \
	-I./dist/include -L./dist/lib \
	-lavcodec -lavformat -lavutil -lswscale -lswresample -lx264 -lvpx \
	-s INITIAL_MEMORY=36700160 \
	-s ALLOW_MEMORY_GROWTH=1 \
	-lembind -lworkerfs.js \
    -pthread \
	-flto \
	-o web/assets/out.js

init:
	bash scripts/setup_dist.sh