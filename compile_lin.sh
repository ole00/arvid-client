# builds on all flavours of linux 
SRC_DIR=src
OSDEP_DIR=src-osdep/linux
OUT_DIR=bin
CFLAGS="-Wno-implicit-function-declaration \
       -I${OSDEP_DIR} \
       -Iinclude \
       "
LDFLAGS="-L./ -larvid_client -lz -lpthread"

rm libarvid_client.*
rm -rf ${OUT_DIR}
mkdir -p ${OUT_DIR}

# compile arvid-client library
gcc -c ${CFLAGS} ${OSDEP_DIR}/tsync.c -o ${OUT_DIR}/tsync.o
gcc -c ${CFLAGS} ${SRC_DIR}/crc.c -o ${OUT_DIR}/crc.o
gcc -c ${CFLAGS} -O2 ${SRC_DIR}/arvid_client.c -o ${OUT_DIR}/arvid_client.o
ar rcs libarvid_client.a ${OUT_DIR}/tsync.o ${OUT_DIR}/crc.o ${OUT_DIR}/arvid_client.o 

# compile tools
gcc ${CFLAGS} -o ${OUT_DIR}/demo ${SRC_DIR}/demo.c ${LDFLAGS}
#gcc ${CFLAGS} -o ${OUT_DIR}/sync ${SRC_DIR}/sync.c ${LDFLAGS}
gcc ${CFLAGS} -o ${OUT_DIR}/fw_upload ${SRC_DIR}/fw_upload.c ${LDFLAGS}
gcc ${CFLAGS} -o ${OUT_DIR}/arvid_poweroff ${SRC_DIR}/arvid_poweroff.c ${LDFLAGS}
