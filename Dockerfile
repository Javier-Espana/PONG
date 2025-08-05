FROM debian:bullseye-slim

# Variables de entorno para SDL y X11
ENV DISPLAY=${DISPLAY}
ENV XDG_RUNTIME_DIR=/tmp/runtime
ENV SDL_VIDEODRIVER=x11
ENV LIBGL_ALWAYS_SOFTWARE=1
# Deshabilitar MIT-SHM para evitar problemas en Docker
ENV SDL_VIDEO_X11_FORCE_EGL=0
ENV SDL_VIDEO_X11_DGAMOUSE=0
ENV QT_X11_NO_MITSHM=1
ENV _X11_NO_MITSHM=1
ENV _MITSHM=0

# 1. Configure reliable package sources with fallbacks
RUN echo "deb http://deb.debian.org/debian bullseye main" > /etc/apt/sources.list && \
    echo "deb http://deb.debian.org/debian bullseye-updates main" >> /etc/apt/sources.list && \
    echo "deb http://security.debian.org/debian-security bullseye-security main" >> /etc/apt/sources.list && \
    echo 'Acquire::Retries "5"; Acquire::http::Timeout "20";' > /etc/apt/apt.conf.d/99-retry-timeout

# 2. Update and install minimal requirements first
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    ca-certificates \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# 3. Install build tools
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    g++ \
    make \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# 4. Install SDL development libraries
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-ttf-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# 5. (Optional) Install SDL mixer if needed
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    libsdl2-mixer-dev \
    || echo "Warning: Could not install libsdl2-mixer-dev" \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Application setup
COPY ./src /app/src
COPY ./res /app/res
WORKDIR /app
RUN mkdir -p bin && \
    g++ -o bin/pong src/pong.cpp \
    -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf \
    -lpthread
CMD ["/app/bin/pong"]