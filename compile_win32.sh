# cross-compile Win32 libs and tool under Linux

PREFIX=~/opt/mingw32/bin/i686-w64-mingw32-

SRC_DIR=src
OSDEP_DIR=src-osdep/windows
OUT_DIR=bin
CFLAGS="-Wno-implicit-function-declaration \
       -I${OSDEP_DIR} \
       -Iinclude \
       -DMINGW \
       -Iinclude/zlib \
       -O2 \
       "
LDFLAGS="-L./ -larvid_client -L./lib-osdep/win32 -lz -lws2_32"

echo "prefix = ${PREFIX}"
rm libarvid_client.*
rm -rf ${OUT_DIR}
mkdir -p ${OUT_DIR}

# compile arvid-client library
${PREFIX}gcc -c ${CFLAGS} ${OSDEP_DIR}/tsync.c -o ${OUT_DIR}/tsync.o
${PREFIX}gcc -c ${CFLAGS} ${SRC_DIR}/crc.c -o ${OUT_DIR}/crc.o
${PREFIX}gcc -c ${CFLAGS} -O2 ${SRC_DIR}/arvid_client.c -o ${OUT_DIR}/arvid_client.o
${PREFIX}ar rcs libarvid_client.a ${OUT_DIR}/tsync.o ${OUT_DIR}/crc.o ${OUT_DIR}/arvid_client.o

# compile tools
${PREFIX}gcc ${CFLAGS} -o ${OUT_DIR}/demo.exe ${SRC_DIR}/demo.c ${LDFLAGS}
${PREFIX}gcc ${CFLAGS} -o ${OUT_DIR}/fw_upload.exe ${SRC_DIR}/fw_upload.c ${LDFLAGS}
