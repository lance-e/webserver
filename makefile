CC=g++
SRCS=$(wildcard *.cpp */*.cpp)
OBJS=$(patsubst %.cpp, %.o, $(SRCS))
FLAG=-g
NAME=$(wildcard *.cpp)
TARGET=$(patsubst %.cpp, %, $(NAME))

$(TARGET):$(OBJS)
	$(CC) -o $@ $^ $(FLAG)

%.o:%.c
	$(CC) -o $@ -c $< -g

clean:
	rm -rf $(TARGET) $(OBJS)
