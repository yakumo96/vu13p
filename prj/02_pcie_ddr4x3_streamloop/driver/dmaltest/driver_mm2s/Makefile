CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -fPIC
LDFLAGS = -lpthread -ldl

# ターゲットファイル
LIB_TARGET = libaxidma_mm2s.so
TEST_TARGET = play_test
TEST_SO_TARGET = play_test_so

# ソースファイル
LIB_SRCS = axidma_mm2s.c
TEST_SRCS = play_test.c
TEST_SO_SRCS = play_test_so.c

# ターゲットルール
all: $(LIB_TARGET) $(TEST_TARGET) $(TEST_SO_TARGET)

# 共有ライブラリをビルド
$(LIB_TARGET): $(LIB_SRCS)
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)

# テストプログラムをビルド（直接リンク）
$(TEST_TARGET): $(TEST_SRCS)
	$(CC) $(CFLAGS) -o $@ $< -L. -laxidma_mm2s -Wl,-rpath,. $(LDFLAGS)

# テストプログラムをビルド（動的ロード）
$(TEST_SO_TARGET): $(TEST_SO_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# クリーンルール
clean:
	rm -f $(LIB_TARGET) $(TEST_TARGET) $(TEST_SO_TARGET)

# インストールルール
install: $(LIB_TARGET)
	install -m 755 $(LIB_TARGET) /usr/local/lib/
	ldconfig

# 実行ルール
run: $(TEST_TARGET)
	./$(TEST_TARGET)

run_so: $(TEST_SO_TARGET)
	./$(TEST_SO_TARGET)

.PHONY: all clean install run run_so

