SGXLKL_STARTER=../../../build/sgx-lkl-run-oe
# ----------------------------------------------------------------
EXECUTION_TIMEOUT=12000
SGXLKL_ENV=SGXLKL_VERBOSE=1 SGXLKL_KERNEL_VERBOSE=1
SGXLKL_HW_PARAMS=--hw-debug
# ----------------------------------------------------------------
PROG_NAME=hello
# ----------------------------------------------------------------
IMG_NAME=haohua-test/$(PROG_NAME)
ROOT_FS=$(PROG_NAME).img
ALPINE_TAR=$(PROG_NAME).tar.gz
ALPINE_CONTAINER_CID=$(PROG_NAME).cid
MOUNTPOINT=/media/ext4disk
ENTRY_PROG=/$(PROG_NAME)
IMAGE_SIZE_MB=150

all: $(ROOT_FS)

gettimeout:
	@echo ${EXECUTION_TIMEOUT}
	
# create a container and get its id 
$(ALPINE_CONTAINER_CID) : 
	rm -f $(ALPINE_CONTAINER_CID)
	@echo "Building Docker image..."
	DOCKER_BUILDKIT=1 docker build --build-arg STRONGBOX_FLAGS="${STRONGBOX_FLAGS}" --rm=true -t $(IMG_NAME) ${DOCKERFILE_NAME} .
	docker run --cidfile $(ALPINE_CONTAINER_CID) -d $(IMG_NAME) bash 

# cat tar based on the cotainer 
$(ALPINE_TAR) : $(ALPINE_CONTAINER_CID)
	docker export -o $(ALPINE_TAR) $(shell cat $(ALPINE_CONTAINER_CID))

# extract the files from the container, and put them to a disk image (ext4)
$(ROOT_FS) : $(ALPINE_TAR)
	dd if=/dev/zero of="${ROOT_FS}" count=$(IMAGE_SIZE_MB) bs=1M
	mkfs.ext4 $(ROOT_FS)
	mkdir -p $(MOUNTPOINT)
	mount -t ext4 -o loop $(ROOT_FS) $(MOUNTPOINT)
	tar -C $(MOUNTPOINT) -xf $(ALPINE_TAR) 
	umount $(MOUNTPOINT)
	chown $(USER) "$(ROOT_FS)"

run: run-hw 

run-hw: ${ROOT_FS}
	SGXLKL_ETHREADS=1 SGXLKL_ETHREADS_AFFINITY=3 $(SGXLKL_STARTER) $(SGXLKL_HW_PARAMS) $(ROOT_FS) /helloworld

run-hw-verbose: ${ROOT_FS}
	SGXLKL_PRINT_APP_RUNTIME=1 SGXLKL_VERBOSE=1 SGXLKL_KERNEL_VERBOSE=1 SGXLKL_ETHREADS=1 SGXLKL_ETHREADS_AFFINITY=3 SGXLKL_ESPINS=1000000 $(SGXLKL_STARTER) $(SGXLKL_HW_PARAMS) $(ROOT_FS) /helloworld

clean:
	rm -f $(ROOT_FS) $(ALPINE_CONTAINER_CID) $(ALPINE_TAR)


