all:
	i686-w64-mingw32-g++ falling-cubes-demo.cpp -o falling-cubes.exe -lopengl32 -lglu32 -lgdi32 -mwindows -lwinpthread -static-libgcc -static-libstdc++
	cp /usr/i686-w64-mingw32/sys-root/mingw/bin/libwinpthread-1.dll .

clean:
	rm *.exe libwinpthread-1.dll

run:
	wine falling-cubes.exe
