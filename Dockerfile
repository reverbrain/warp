FROM reverbrain/xenial-dev

#RUN	echo "deb http://repo.reverbrain.com/xenial/ current/amd64/" > /etc/apt/sources.list.d/reverbrain.list && \
#	echo "deb http://repo.reverbrain.com/xenial/ current/all/" >> /etc/apt/sources.list.d/reverbrain.list && \
#	apt-get install -y curl tzdata && \
#	cp -f /usr/share/zoneinfo/posix/W-SU /etc/localtime && \
#	curl http://repo.reverbrain.com/REVERBRAIN.GPG | apt-key add - && \
#	apt-get update && \
#	apt-get upgrade -y && \
#	apt-get install -y git g++ liblz4-dev libsnappy-dev zlib1g-dev libbz2-dev libgflags-dev libjemalloc-dev && \
#	apt-get install -y cmake debhelper cdbs devscripts && \
#	git config --global user.email "zbr@ioremap.net" && \
#	git config --global user.name "Evgeniy Polyakov"

RUN	apt-get install -y libboost-system-dev libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev && \
	apt-get install -y libmsgpack-dev libswarm3-dev libthevoid3-dev ribosome-dev && \
	cd /tmp && \
	rm -rf warp && \
	git clone https://github.com/reverbrain/warp && \
	cd warp && \
	git branch -v && \
	dpkg-buildpackage -b && \
	dpkg -i ../warp_*.deb ../warp-dev_*.deb && \
	echo "Warp package has been updated and installed" && \
    	rm -rf /var/lib/apt/lists/*

EXPOSE 8101
