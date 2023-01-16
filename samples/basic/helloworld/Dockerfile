FROM alpine:3.16

ARG STRONGBOX_FLAGS
RUN apk add --no-cache musl-dev bash gcc 
ADD *.c /
ADD app/helloworld.txt /app/helloworld.txt
RUN gcc -o helloworld helloworld.c
