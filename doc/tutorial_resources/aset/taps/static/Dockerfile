# Static CIR publisher for vrtsim --vrtsim.taps-socket
# Uses shared taps/common/publish_taps.py --source static.
FROM ubuntu:noble
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3 python3-pip \
        flatbuffers-compiler \
        libnanomsg5 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/taps
COPY taps/common/taps.fbs taps/common/set-gain taps/common/publish_taps.py taps/common/requirements.txt ./
RUN flatc --python taps.fbs \
    && pip3 install --no-cache-dir --break-system-packages -r requirements.txt \
    && chmod +x set-gain publish_taps.py \
    && ln -sf /opt/taps/set-gain /usr/local/bin/set-gain \
    && ln -sf /opt/taps/publish_taps.py /usr/local/bin/publish_taps

ENV TAPS_DL_CTL_SOCK=/tmp/vrtsim_taps_dl.ctl
ENV TAPS_UL_CTL_SOCK=/tmp/vrtsim_taps_ul.ctl
ENTRYPOINT ["python3", "/opt/taps/publish_taps.py"]
CMD ["--dual", "--source", "static"]
