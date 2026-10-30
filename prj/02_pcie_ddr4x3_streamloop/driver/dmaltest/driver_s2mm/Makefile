# AXI DMA S2MM ドライバとテストプログラムの Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -fPIC
LDFLAGS = -lpthread -ldl

# ターゲットファイル
LIB_SHARED = libaxidma_s2mm.so
LIB_STATIC = libaxidma_s2mm.a
TEST_DIRECT = recv_test
TEST_SO = recv_test_so

# ソースファイルとオブジェクトファイル
SRC = axidma_s2mm.c
OBJ = $(SRC:.c=.o)
TEST_SRC = recv_test.c
TEST_OBJ = $(TEST_SRC:.c=.o)
TEST_SO_SRC = recv_test_so.c
TEST_SO_OBJ = $(TEST_SO_SRC:.c=.o)

# デフォルトターゲット
all: $(LIB_SHARED) $(LIB_STATIC) $(TEST_DIRECT) $(TEST_SO)

# 共有ライブラリをビルド
$(LIB_SHARED): $(OBJ)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

# 静的ライブラリをビルド
$(LIB_STATIC): $(OBJ)
	ar rcs $@ $^

# 直接リンク版テストプログラムをビルド（修正後はライブラリファイルに依存しない）
$(TEST_DIRECT): $(TEST_OBJ) $(SRC)
	$(CC) -o $@ $(TEST_OBJ) $(SRC) $(LDFLAGS)

# 共有ライブラリ版テストプログラムをビルド
$(TEST_SO): $(TEST_SO_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# コンパイルルール
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# クリーン
clean:
	rm -f $(OBJ) $(TEST_OBJ) $(TEST_SO_OBJ) $(LIB_SHARED) $(LIB_STATIC) $(TEST_DIRECT) $(TEST_SO)

# インストール
install: $(LIB_SHARED) $(LIB_STATIC)
	install -d $(DESTDIR)/usr/local/lib
	install -m 755 $(LIB_SHARED) $(DESTDIR)/usr/local/lib
	install -m 644 $(LIB_STATIC) $(DESTDIR)/usr/local/lib
	install -d $(DESTDIR)/usr/local/include
	install -m 644 axidma_s2mm.h $(DESTDIR)/usr/local/include
	ldconfig

.PHONY: all clean install

