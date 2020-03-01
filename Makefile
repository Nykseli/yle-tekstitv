
all:
	gcc -Ithird_party/tidy-html5/include src/main.c third_party/tidy-html5/build/cmake/libtidys.a

# https://stackoverflow.com/a/15065359 < shared linking
# shared linking only works on dev
shared:
	gcc -Ithird_party/tidy-html5/include -Lthird_party/tidy-html5/build/cmake/ -Wl,-Rthird_party/tidy-html5/build/cmake/ src/main.c -ltidy

clean:
	rm a.out

.PHONY: all clean
