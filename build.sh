#!/bin/bash

# By default compile kernel for S8 European model (g950x).
# If you need to build for S8+, just uncomment model "g955x" and comment model "g950x".

# SETUP

### S8 ###
MODEL=g950x

### S8 Korea version ###
#MODEL=g950x_kor

### S8+ ###
#MODEL=g955x

### S8+ Korea ###
#MODEL=g955x_kor

### Note 8 ###
#MODEL=n950x

### Note 8 Korea ###
#MODEL=n950x_kor

SOURCE_PATH=$HOME/Samsung_dreamlte_Kernel
N=$(nproc)
OUTPUT=$HOME/a2n_kernel_$MODEL_9.x
AIK=$HOME/AIK-Linux

	cd $SOURCE_PATH
	rm arch/arm64/boot/dts/exynos/*dtb*
	ARCH=arm64 scripts/kconfig/merge_config.sh arch/arm64/configs/g950x_defconfig arch/arm64/configs/$MODEL_defconfig
	make -j$N $@

	# copy modules
	cp fs/cifs/cifs.ko $OUTPUT/system/lib/modules
	cp crypto/md4.ko $OUTPUT/system/lib/modules
	cp fs/fscache/fscache.ko $OUTPUT/system/lib/modules
	cp net/dns_resolver/dns_resolver.ko $OUTPUT/system/lib/modules
	cp drivers/usb/gadget/function/usb_f_mtp_samsung.ko $OUTPUT/system/lib/modules
	cp drivers/usb/gadget/function/usb_f_ptp.ko $OUTPUT/system/lib/modules
	cp fs/ntfs/ntfs.ko $OUTPUT/system/lib/modules

	cp fs/nfs_common/grace.ko $OUTPUT/system/lib/modules
	cp fs/nfs/nfs.ko $OUTPUT/system/lib/modules
	cp fs/nfs/nfsv2.ko $OUTPUT/system/lib/modules
	cp fs/nfs/nfsv3.ko $OUTPUT/system/lib/modules
	cp fs/nfs/nfsv4.ko $OUTPUT/system/lib/modules
	cp fs/lockd/lockd.ko $OUTPUT/system/lib/modules
	cp net/sunrpc/sunrpc.ko $OUTPUT/system/lib/modules
	cp net/sunrpc/auth_gss/auth_rpcgss.ko $OUTPUT/system/lib/modules
	cp lib/oid_registry.ko $OUTPUT/system/lib/modules

	cp net/ipv4/fou.ko $OUTPUT/system/lib/modules
	cp net/ipv4/udp_tunnel.ko $OUTPUT/system/lib/modules
	cp net/ipv6/ip6_udp_tunnel.ko $OUTPUT/system/lib/modules
	cp net/wireguard/wireguard.ko $OUTPUT/system/lib/modules
	cp net/l2tp/l2tp_core.ko $OUTPUT/system/lib/modules

	cp arch/arm64/boot/Image $AIK/split_img/boot.img-kernel

	./tools/dtbtool -o $AIK/split_img/boot.img-dt arch/arm64/boot/dts/exynos/

	cd $AIK/

	./repackimg.sh --nosudo

	cp image-new.img $OUTPUT/boot.img

	cd $OUTPUT/

	rm *.zip

	zip -r a2n_kernel_$MODEL_9.x_user_build.zip META-INF system boot.img

	md5sum *.zip > *.md5

