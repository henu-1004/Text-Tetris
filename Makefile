# Makefile for Tetris Project

# 컴파일러
CC = gcc

# 소스 및 실행파일 이름
SRC = tetris.c
EXE = tetris.exe

# 공통 컴파일 옵션
CFLAGS = -std=c99 -Wall -O2

# 운영체제에 따른 실행파일 이름 결정
ifeq ($(OS),Windows_NT)
    TARGET = $(EXE)
else
    TARGET = tetris
endif

# 기본 빌드
all: $(TARGET)

$(EXE): $(SRC)
	$(CC) $(CFLAGS) -o $(EXE) $(SRC)

tetris: $(SRC)
	$(CC) $(CFLAGS) -o tetris $(SRC)

# 실행파일 삭제
clean:
	-del $(EXE) 2>nul || rm -f $(EXE) tetris

.PHONY: all clean
