nelly@nelly-NUC7PJYHN:~/sgx-lkl/tests/tools/sgx-lkl-cfg/create$ make update-ref-cfg
/home/nelly/sgx-lkl/tools/sgx-lkl-disk create --size=100M --docker=./Dockerfile rootfs.img
Using detected host DNS server in /etc/resolv.conf: 192.168.101.1
Creating rootfs.img from Dockerfile ./Dockerfile...
Building Docker image...
 => => writing image sha256:e483256eab66365bca13d27c4dc89556bc15e2157c2588c84e75d65369406846                                                                                                                                              0.0s 
 => => naming to docker.io/library/60dbe983-b05c-4713-bfd1-352a6685dbf6                                                                                                                                                                   0.0s 
Creating disk image file from Docker container...
Succesfully created rootfs.img.
cp rootfs.img datafs.img
/home/nelly/sgx-lkl/tools/sgx-lkl-cfg create \
  --disk rootfs.img datafs.img \
  --host-cfg host-config.json.tmp --enclave-cfg enclave-config.json.tmp
Host and enclave configuration files successfully created.

You should now do the following:
- Review the generated files.
- Change the default mount paths for the extra disks from /data_<i> if needed.
- Change the args" fields in the enclave configuration if needed.
- Change any additional fields if needed (see documentation).
/home/nelly/sgx-lkl/tools/sgx-lkl-cfg create \
  --disk rootfs.img datafs.img \
  --host-cfg host-config-complete.json.tmp --enclave-cfg enclave-config-complete.json.tmp \
  --complete
Added $.format_version: 1
Added $.mode: "hw_release"
Added $.net_ip4: "10.0.1.1"
Added $.net_gw4: "10.0.1.254"
Added $.net_mask4: "24"
Added $.hostname: "lkl"
Added $.hostnet: false
Added $.tap_mtu: 0
Added $.wg.ip: "10.0.2.1"
Added $.wg.listen_port: 56002
Added $.wg.key: null
Added $.wg.peers: []
Added $.ethreads: 1
Added $.max_user_threads: 256
Added $.espins: 500
Added $.esleep: 16000
Added $.clock_res: [{"resolution": "0000000000000001"}, {"resolution": "0000000000000001"}, {"resolution": "0000000000000000"}, {"resolution": "0000000000000000"}, {"resolution": "0000000000000001"}, {"resolution": "00000000003d0900"}, {"resolution": "00000000003d0900"}, {"resolution": "0000000000000001"}]
Added $.stacksize: 524288
Added $.mmap_files: "shared"
Added $.oe_heap_pagecount: 8192
Added $.fsgsbase: true
Added $.verbose: false
Added $.kernel_verbose: false
Added $.kernel_cmd: "mem=32M"
Added $.sysctl: null
Added $.swiotlb: true
Added $.host_import_env: []
Added $.exit_status: "full"
Added $.root.key: ""
Added $.root.key_id: null
Added $.root.roothash: null
Added $.root.roothash_offset: 0
Added $.root.overlay: false
Added $.image_sizes.num_heap_pages: 262144
Added $.image_sizes.num_stack_pages: 1024
Added $.io.network: true
Added $.io.block: true
Added $.io.console: true
Added $.sgxstep_attack_delay: 500
Added $.root.key: ""
Added $.root.overlay: false
Added $.root.readonly: false
Added $.root.verity: ""
Added $.root.verity_offset: ""
Added $.verbose: true
Added $.ethreads_affinity: ""
Added $.tap_device: ""
Added $.tap_offload: true
Host and enclave configuration files successfully created.

You should now do the following:
- Review the generated files.
- Change the default mount paths for the extra disks from /data_<i> if needed.
- Change the args" fields in the enclave configuration if needed.
- Change any additional fields if needed (see documentation).
cp enclave-config.json.tmp enclave-config-ref.json
cp enclave-config-complete.json.tmp enclave-config-complete-ref.json