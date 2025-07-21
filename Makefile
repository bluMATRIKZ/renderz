all:
	i686-w64-mingw32-g++ falling-cubes-demo.cpp -o falling-cubes.exe -lopengl32 -lglu32 -lgdi32 -mwindows -lwinpthread

clean:
	rm *.exe

run:
	wine falling-cubes.exe
