#include "../includes/Server.hpp"

int main() {
    //socket vory framea sarqum serveri hamar veradarcnuma fd sistemayum resursa sarqum
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }
    //es struckturayi haytararumna classi mej erevi qcenq heto inqy grum enq vor arden bolor texery ogtagorcenq mejy 2 bana pahum inch porti vra ashxati u vor ipv4ov ashxati
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);
    // bind ynunuma vor socketic vor portum inch chapov kpni
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    // listen spasoxakan rejima qcum 2 parametra ynudum mer sockety u 2rdy qani texic en spasum 10 randoma
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Server started on port 8080\n";

    while (true) {
        //accept tak 3 parametra 1 mer socketna 2rdy clienti hascen karanq NULL tanq avtomat berelua 3rdnel chapy eli avtomat berelua arden inqy sockety bacuma clienti hamar
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        //recv clienti hamar tvyalnerna kardum vor 1 ynduma te vor socketic karda 2 vortex gri 3 inchqan maximum karda 4 flaga chem nayel
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        recv(client_fd, buffer, sizeof(buffer), 0);

        std::cout << "Client request:\n" << buffer << std::endl;
        // send clentin tvyalner uxxarkum nuyn recvi parametrnerna yndunum
        std::string request[buffer];
        parse_f(request);
        send(client_fd, response, strlen(response), 0);
        close(client_fd);
    }

    close(server_fd);
}