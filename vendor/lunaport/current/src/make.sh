i586-mingw32msvc-windres resource.rc -O coff -o resource.o
i586-mingw32msvc-g++ Lobby.cpp HTTP.cpp Session.cpp StageManager.cpp Crc32.cpp ConManager.cpp LunaPort.cpp resource.o -o ../LunaPort.exe -lwinmm -lcomdlg32 -lm -lcurl -lz -lwldap32 -lws2_32 -O3 -DCURL_STATICLIB
rm -f resource.o
i586-mingw32msvc-strip -s ../LunaPort.exe
