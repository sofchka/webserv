NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRCS = srcs/Server.cpp srcs/main.cpp srcs/parse_f.cpp srcs/make_response.cpp srcs/utils.cpp srcs/get_type.cpp srcs/config/config.cpp srcs/config/utils.cpp srcs/config/parse.cpp 

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	@mkdir -p web/uploads
	@mkdir -p web/cgi-bin
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)


%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all
