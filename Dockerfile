FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    libcurl4-openssl-dev libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . -j$(nproc)

RUN cp build/uslugi_web /app/uslugi_web

EXPOSE 8080
CMD ["./uslugi_web"]
