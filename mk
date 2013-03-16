#!/bin/sh

cc -std=c99 -Wall -g -o fastbrt $(pkg-config --cflags --libs glib-2.0) fastbrt.c
