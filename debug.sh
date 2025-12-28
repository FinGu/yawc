ninja -C build
valgrind --leak-check=full --show-leak-kinds=definite build/yawc -w build/default-wm/libdefault_wm.so 2> error.log
