#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <dirent.h>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>

using namespace std;

struct files_recieved
{
    int id;
    int PORT;
    int unique_id;
    string file;
    int depth;
};

struct neighbour
{
    int id;
    int PORT;
    int unique_private_id;
    int socket = 0;
    int socket_listen = 0;
    int message_recieved = 0;
};

int id;
const char *PORT;
int unique_private_id;
vector<string> files_searching;
vector<files_recieved> files_recv;
vector<neighbour> neighbours;
int fd_count = 0;
int fd_size = 5;
struct pollfd *pfds = (pollfd *)malloc(sizeof *pfds * fd_size);

// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// Return a listening socket
int get_listener_socket(void)
{
    int listener; // Listening socket descriptor
    int yes = 1;  // For setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0)
    {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next)
    {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0)
        {
            continue;
        }

        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
        {
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai); // All done with this

    // If we got here, it means we didn't get bound
    if (p == NULL)
    {
        return -1;
    }

    // Listen
    if (listen(listener, 10) == -1)
    {
        return -1;
    }

    return listener;
}

// Add a new file descriptor to the set
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size)
    {
        *fd_size *= 2; // Double it

        *pfds = (pollfd *)realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

// Remove an index from the set
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count - 1];

    (*fd_count)--;
}

bool compareFunction(std::string a, std::string b)
{
    transform(a.begin(), a.end(), a.begin(), ::tolower);
    transform(b.begin(), b.end(), b.begin(), ::tolower);
    return a < b;
}


int get_index_neighbour_by_socket_listen(int socket)
{
    int index = 0;
    for (size_t i = 0; i < neighbours.size(); i++)
    {
        if (socket == neighbours[i].socket_listen)
        {
            return i;
        }
    }
    return -1;
}

int get_index_neighbour_by_socket(int socket)
{
    int index = 0;
    for (size_t i = 0; i < neighbours.size(); i++)
    {
        if (socket == neighbours[i].socket)
        {
            return i;
        }
    }
    return -1;
}

int get_index_neighbour_by_id(int id)
{
    int index = 0;
    for (size_t i = 0; i < neighbours.size(); i++)
    {
        if (id == neighbours[i].id)
        {
            return i;
        }
    }
    return -1;
}

int index_pfds(int socket)
{
    for (size_t i = 0; i < fd_size; i++)
    {
        if (pfds[i].fd == socket)
        {
            return i;
        }
    }
    return -1;
}
// Main
void tokenize(string s, vector<string> &rslt, string del = " ")
{
    int start = 0;
    int end = s.find(del);
    while (end != -1)
    {
        // cout << s.substr(start, end - start) << " ";
        rslt.push_back(s.substr(start, end - start));
        start = end + del.size();
        end = s.find(del, start);
    }
    // cout << s.substr(start, end - start);
    rslt.push_back(s.substr(start, end - start));
}

int main(int argc, char *argv[])
{
    // Directory read

    string path_to_directory = argv[2];
    const char *path = path_to_directory.c_str();

    DIR *dir;
    struct dirent *diread;
    vector<string> files;

    if ((dir = opendir(path)) != nullptr)
    {
        while ((diread = readdir(dir)) != nullptr)
        {
            files.push_back(diread->d_name);
        }
        closedir(dir);
    }
    else
    {
        perror("opendir");
        return EXIT_FAILURE;
    }

    for (auto i = 0; i != files.size(); i++)
    {
        if (files[i] == "." || files[i] == "..")
        {
            // cout<<files[i];
            files.erase(files.begin() + i);
            i--;
        }
    }
    std::sort(files.begin(), files.end(), compareFunction);
    // for (size_t i = 0; i < files_searching.size(); i++)
    // {
    //     cout<<files[i]<<endl;
    // }

    for (auto file : files)
        cout << file << endl;

    // ---------------------------------------------------------------------
    // ---------------------------------------------------------------------
    // ---------------------------------------------------------------------
    // region
    fstream file;
    string word;
    file.open(argv[1]);
    vector<string> config;

    while (file >> word)
    {
        // displaying content
        config.push_back(word);
    }
    file.close();

    id = stoi(config[0]);
    PORT = config[1].c_str();
    unique_private_id = stoi(config[2]);

    int index = 4;
    for (index = 4; index < 4 + 2 * stoi(config[3]); index += 2)
    {
        // vector<int> temp{stoi(config[index]), stoi(config[index + 1])};
        neighbour temp;
        temp.id = stoi(config[index]);
        temp.PORT = stoi(config[index + 1]);
        neighbours.push_back(temp);
    }

    for (size_t i = index + 1; i < config.size(); i++)
    {
        // cout<<config[i]<<"aaaaaaaaaaaa";
        files_searching.push_back(config[i]);
    }
    sort(files_searching.begin(),files_searching.end());
    for (size_t i = 0; i < files_searching.size(); i++)
    {
        files_recieved temp;
        temp.file = files_searching[i];
        temp.depth = 0;
        temp.unique_id = 0;
        files_recv.push_back(temp);
    }

    // endregion
    //---------------------------------------------------------------

    int listener; // Listening socket descriptor

    int newfd;                          // Newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;

    char buf[1024]; // Buffer for client data

    char remoteIP[INET6_ADDRSTRLEN];
    listener = get_listener_socket();

    if (listener == -1)
    {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }

    // Add the listener to set
    pfds[0].fd = listener;
    pfds[0].events = POLLIN; // Report ready to read on incoming connection

    //----------------CLIENT--------------------------

    string port_temp;
    port_temp.append(PORT);

    fd_count = 1; // For the listener

    for (size_t i = 0; i < neighbours.size(); i++)
    {
        struct sockaddr_in serv_addr;
        // char buffer[1024] = {0};
        if ((neighbours[i].socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("\n Socket creation error \n");
            return -1;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(neighbours[i].PORT);

        // Convert IPv4 and IPv6 addresses from text to binary
        // form
        if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
        {
            printf(
                "\nInvalid address/ Address not supported \n");
            return -1;
        }
        for (;;)
        {
            if (connect(neighbours[i].socket, (struct sockaddr *)&serv_addr,
                        sizeof(serv_addr)) < 0)
            {
                // printf("\nConnection Failed \n");
                // return -1;
            }
            else
                break;
        }
        add_to_pfds(&pfds, neighbours[i].socket, &fd_count, &fd_size);
    }

    //------------------------------------------------

    int total_listened1 = 0;
    int total_listened2 = 0;

    ///////////////////////////////////////////////////////////////////
    for (size_t i = 0; i < neighbours.size(); i++)
    {
        addrlen = sizeof remoteaddr;
        newfd = accept(listener,
                       (struct sockaddr *)&remoteaddr,
                       &addrlen);

        if (newfd == -1)
        {
            perror("accept");
        }
        else
        {
            add_to_pfds(&pfds, newfd, &fd_count, &fd_size);
        }
    }
    for (size_t i = 0; i < neighbours.size(); i++)
    {
        string s = to_string(id) + "_";
        const char *buf_temp = s.c_str();
        // cout<<endl;
        send(neighbours[i].socket, buf_temp, strlen(buf_temp), 0);
    }
    for (size_t i = 1 + neighbours.size(); i < fd_count; i++)
    {
        char buf_temp[256];
        int nbytes = recv(pfds[i].fd, buf_temp, sizeof buf_temp, 0);
        neighbours[get_index_neighbour_by_id(stoi(strtok(buf_temp, "_")))].socket_listen = pfds[i].fd;
        //
        string s = "1@" + to_string(id) + '_' + to_string(unique_private_id) + '_' + port_temp + '#';
        char buf_temp1[1024];
        for (size_t i = 0; i < s.size(); i++)
        {
            buf_temp1[i] = s[i];
        }
        // cout<<"blah"<<sizeof hello<<"blah"<<endl;
        send(pfds[i].fd, buf_temp1, 1024, 0);
    }

    //////////////////////////////////////////////////////////////////
    // Main loop
    for (;;)
    {
        //cout << "final" << total_listened1 << "_" << total_listened2 << endl;
        if (total_listened1 == neighbours.size() && total_listened2 == neighbours.size())
            break;
        // std::cout << "abc";
        int poll_count = poll(pfds, fd_count, -1);

        if (poll_count == -1)
        {
            perror("poll");
            exit(1);
        }

        // Run through the existing connections looking for data to read
        for (int i = 0; i < fd_count; i++)
        {

            // Check if someone's ready to read
            if (pfds[i].revents & POLLIN)
            { // We got one!!

                if (pfds[i].fd == listener)
                {
                }
                else if (get_index_neighbour_by_socket(pfds[i].fd) == -1)
                {
                    // SERVER
                    //  If not the listener, we're just a regular client
                    int nbytes = recv(pfds[i].fd, buf, 1024, 0);

                    if (nbytes <= 0)
                    {
                        // Got error or connection closed by client
                        if (nbytes == 0)
                        {
                            cout << "Connection closed" << endl;
                            // printf("pollserver: socket %d hung up\n", sender_fd);
                        }
                        else
                        {
                            perror("recv222");
                        }

                        close(pfds[i].fd); // Bye!

                        del_from_pfds(pfds, i, &fd_count);
                    }
                    else
                    {
                        string s;
                        s.append(strtok(buf, "#"));
                        vector<string> rslt;
                        tokenize(s, rslt, "@");
                        //cout << "Server:" << rslt[0] << "  Recieved:" << rslt[1] << "   From:" << neighbours[get_index_neighbour_by_socket_listen(pfds[i].fd)].id << endl;
                        int index = stoi(rslt[0]);
                        switch (index)
                        {
                        case 2:
                        {
                            // cout<<"Connected to ";
                            // cout<<rslt[0];
                            // cout<<  " with unique-ID "  ;
                            // cout<<rslt[1];
                            // cout<< " on port ";
                            // cout<<rslt[2];
                            // cout<<endl;
                            // We got some good data from a client
                            vector<string> recieved;
                            tokenize(rslt[1], recieved, "_");

                            string sending = "2@";
                            sending.append(to_string(neighbours[get_index_neighbour_by_socket_listen(pfds[i].fd)].id) + "_");
                            sending.append(rslt[1]);
                            // cout<<"dddddddddddddd"<<sending<<endl;
                            sending.append("#");
                            char buf_temp[1024];
                            for (size_t i = 0; i < sending.size(); i++)
                            {
                                buf_temp[i] = sending[i];
                            }

                            for (size_t j = 0; j < fd_count; j++)
                            {
                                if (get_index_neighbour_by_socket_listen(pfds[j].fd) != -1)
                                {
                                    if (pfds[i].fd != pfds[j].fd)
                                    {
                                        //cout << "Sending:" << sending << "  To:" << neighbours[get_index_neighbour_by_socket_listen(pfds[j].fd)].id << endl;
                                        send(pfds[j].fd, buf_temp, 1024, 0);
                                    }
                                }
                            }

                            string sending_back;
                            if (neighbours.size() == 1)
                            {
                                sending_back = "4@" + to_string(id) + '_' + to_string(unique_private_id) + '_' + string(PORT);
                            }
                            else
                            {
                                sending_back = "3@" + to_string(id) + '_' + to_string(unique_private_id) + '_' + string(PORT);
                            }
                            for (size_t i = 0; i < recieved.size(); i++)
                            {
                                for (size_t j = 0; j < files.size(); j++)
                                {
                                    if (recieved[i] == files[j])
                                    {
                                        sending_back.append("_" + recieved[i]);
                                        break;
                                    }
                                }
                            }
                            sending_back.append("#");
                            char buf_temp2[1024];
                            for (size_t i = 0; i < sending_back.size(); i++)
                            {
                                buf_temp2[i] = sending_back[i];
                            }
                            //cout << "Sending:" << sending_back << "  To:" << neighbours[get_index_neighbour_by_socket_listen(pfds[i].fd)].id << endl;
                            send(pfds[i].fd, buf_temp2, 1024, 0);

                            if (neighbours.size() == 1)
                            {
                                // del_from_pfds(pfds , i , &fd_count);
                                // total_listened1++;
                            }

                            // cout<<buf_temp<<endl;

                            // send(pfds[i].fd, buf_temp, sending.size(), 0);
                            // del_from_pfds(pfds, i, &fd_count);
                            break;
                        }
                        case 3:
                        {
                            vector<string> recieved;
                            tokenize(rslt[1], recieved, "_");
                            //cout << "neeeeeeed" << rslt[1] << endl;
                            //cout << "3,s" << recieved[0] << recieved[1] << endl;
                            int id_recieved = stoi(recieved[0]);
                            int id_to = stoi(recieved[1]);
                            int index_recieved = get_index_neighbour_by_id(id_recieved);
                            int index_to = get_index_neighbour_by_id(id_to);
                            neighbours[index_to].message_recieved++;

                            string sending;
                            if (neighbours[index_to].message_recieved == neighbours.size() - 1)
                            {
                                sending.append("4@");
                            }
                            else
                            {
                                sending.append("3@");
                            }
                            sending.append(to_string(id_recieved) + '_' + to_string(neighbours[index_recieved].unique_private_id) + '_' + to_string(neighbours[index_recieved].PORT));
                            for (size_t i = 2; i < recieved.size(); i++)
                            {
                                sending.append("_" + recieved[i]);
                            }
                            //cout << "3,c-send" << sending << endl;
                            sending.append("#");
                            char buf_temp[1024];
                            for (size_t i = 0; i < sending.size(); i++)
                            {
                                buf_temp[i] = sending[i];
                            }

                            for (size_t i = 0; i < 50; i++)
                            {
                                //cout << buf_temp[i];
                            }
                            //cout << endl;
                            //cout << "Sending:" << sending << "  To:" << neighbours[get_index_neighbour_by_socket_listen(neighbours[index_to].socket_listen)].id << endl;
                            send(neighbours[index_to].socket_listen, buf_temp, 1024, 0);
                            if (neighbours[index_to].message_recieved == neighbours.size() - 1)
                            {
                                for (size_t k = 0; k < fd_count; k++)
                                {
                                    if (pfds[k].fd == neighbours[index_to].socket_listen)
                                    {
                                        // del_from_pfds(pfds, k, &fd_count);
                                        break;
                                    }
                                }
                                // total_listened1++;
                            }

                            // del_from_pfds(pfds, i, &fd_count);
                            // total_listened2++;
                            break;
                            // del_from_pfds(pfds, i, &fd_count);
                            // total_listened1++;
                        }
                        case 4:
                        {
                            //cout << "END recieved" << neighbours[get_index_neighbour_by_socket_listen(pfds[i].fd)].id << endl;
                            del_from_pfds(pfds, i, &fd_count);
                            total_listened1++;
                        }
                        }
                    }
                }
                else
                {
                    int nbytes = recv(pfds[i].fd, buf, 1024, 0);

                    if (nbytes <= 0)
                    {
                        // Got error or connection closed by client
                        if (nbytes == 0)
                        {
                            cout << "Connection closed" << endl;
                        }
                        else
                        {
                            perror("recv2");
                        }

                        close(pfds[i].fd); // Bye!

                        del_from_pfds(pfds, i, &fd_count);
                    }
                    else
                    {

                        string s;
                        s.append(strtok(buf, "#"));
                        int x = 0;
                        vector<string> rslt;
                        tokenize(s, rslt, "@");
                        //cout << "Client:" << rslt[0] << "  Recieved:" << rslt[1] << "   From:" << neighbours[get_index_neighbour_by_socket(pfds[i].fd)].id << endl;

                        int index = stoi(rslt[0]);
                        switch (index)
                        {
                        case 1:
                        {
                            vector<string> recieved;
                            tokenize(rslt[1], recieved, "_");
                            cout << "Connected to ";
                            cout << recieved[0];
                            cout << " with unique-ID ";
                            cout << recieved[1];
                            cout << " on port ";
                            cout << recieved[2];
                            cout << endl;
                            int temp_index = get_index_neighbour_by_id(stoi(recieved[0]));
                            neighbours[temp_index].PORT = stoi(recieved[2]);
                            neighbours[temp_index].unique_private_id = stoi(recieved[1]);
                            // We got some good data from a client
                            string sending = "2@";
                            for (size_t i = 0; i < files_searching.size(); i++)
                            {
                                sending.append(files_searching[i] + "_");
                            }
                            sending[sending.size() - 1] = '#';
                            char buf_temp[1024];
                            for (size_t i = 0; i < sending.size(); i++)
                            {
                                buf_temp[i] = sending[i];
                                //cout << buf_temp[i];
                            }
                            //cout << endl;
                            // cout<<buf_temp<<endl;
                            //cout << "Sending:" << sending << "  To:" << neighbours[get_index_neighbour_by_socket(pfds[i].fd)].id << endl;
                            send(pfds[i].fd, buf_temp, 1024, 0);
                            // del_from_pfds(pfds, i, &fd_count);
                            break;
                        }
                        case 2:
                        {
                            vector<string> recieved;
                            tokenize(rslt[1], recieved, "_");
                            //cout << "2,C" << recieved[0] << endl;
                            int id_demand = stoi(recieved[0]);
                            string sample1 = "3@";
                            string sending = sample1 + to_string(id) + '_' + to_string(id_demand);
                            // cout<<"blahhhhhh"<<sending<<endl;
                            for (size_t i = 0; i < recieved.size(); i++)
                            {
                                //cout << recieved[i] << "_";
                            }
                            //cout << endl;
                            for (size_t i = 0; i < files.size(); i++)
                            {
                                //cout << files[i] << "_";
                            }
                            //cout << endl;

                            for (size_t i = 1; i < recieved.size(); i++)
                            {
                                for (size_t j = 0; j < files.size(); j++)
                                {
                                    if (recieved[i] == files[j])
                                    {
                                        sending.append("_" + recieved[i]);
                                    }
                                }
                            }
                            sending.append("#");
                            //cout << "Blahhhhhhhhhhhhhh" << sending << endl;
                            char buf_temp[1024];
                            for (size_t i = 0; i < sending.size(); i++)
                            {
                                buf_temp[i] = sending[i];
                                //cout << buf_temp[i];
                            }
                            //cout << endl;
                            //cout << "Sending:" << sending << "  To:" << neighbours[get_index_neighbour_by_socket(pfds[i].fd)].id << endl;
                            send(pfds[i].fd, buf_temp, 1024, 0);
                            // del_from_pfds(pfds, i, &fd_count);
                            // total_listened2++;
                            break;
                        }
                        case 3:
                        {
                            vector<string> recieved;
                            tokenize(rslt[1], recieved, "_");

                            //cout << "3,C" << recieved[0] << recieved[1] << recieved[2] << endl;
                            int id_from = stoi(recieved[0]);
                            int unique_id_from = stoi(recieved[1]);
                            int PORT_from = stoi(recieved[2]);
                            int depth;
                            if (get_index_neighbour_by_id(id_from) == -1)
                            {
                                depth = 2;
                            }
                            else
                            {
                                depth = 1;
                            }
                            for (size_t i = 3; i < recieved.size(); i++)
                            {
                                files_recieved temp;
                                temp.depth = depth;
                                temp.file = recieved[i];
                                temp.id = id_from;
                                temp.PORT = PORT_from;
                                temp.unique_id = unique_id_from;
                                files_recv.push_back(temp);
                                //cout<<"Recieved : "<<temp.file<<"  "<<temp.id<<"  From:"<<neighbours[get_index_neighbour_by_socket(pfds[i].fd)].id <<endl;
                            }
                            break;
                        }
                        case 4:
                        {
                            vector<string> recieved;
                            tokenize(rslt[1], recieved, "_");
                            //cout << "4,C" << recieved[0] << recieved[1] << recieved[2] << endl;
                            int id_from = stoi(recieved[0]);
                            int unique_id_from = stoi(recieved[1]);
                            int PORT_from = stoi(recieved[2]);
                            int depth;
                            if (get_index_neighbour_by_id(id_from) == -1)
                            {
                                depth = 2;
                            }
                            else
                            {
                                depth = 1;
                            }
                            for (size_t i = 3; i < recieved.size(); i++)
                            {
                                files_recieved temp;
                                temp.depth = depth;
                                temp.file = recieved[i];
                                temp.id = id_from;
                                temp.PORT = PORT_from;
                                temp.unique_id = unique_id_from;
                                files_recv.push_back(temp);
                                //cout<<"Recieved : "<<temp.file<<"  "<<temp.id<<"  From:"<<neighbours[get_index_neighbour_by_socket(pfds[i].fd)].id <<endl;
                                
                            }
                            string sending = "4@END#";
                            char buf_temp[1024];
                            for (size_t i = 0; i < sending.size(); i++)
                            {
                                buf_temp[i] = sending[i];
                                // cout<<buf_temp[i];
                            }
                            //cout << "Sending:" << sending << "  To:" << neighbours[get_index_neighbour_by_socket(pfds[i].fd)].id << endl;
                            send(pfds[i].fd, buf_temp, 1024, 0);
                            //cout << "END sent" << neighbours[get_index_neighbour_by_socket(pfds[i].fd)].id << endl;
                            del_from_pfds(pfds, i, &fd_count);
                            total_listened2++;

                            break;
                        }
                        }

                    } // END handle data from client
                }     // END got ready-to-read from poll()
            }         // got ready-to-read from poll()
        }             // END looping through file descriptors
    }                 // END for(;;)--and you thought it would never end!

    for (size_t i = 0; i < files_searching.size(); i++)
    {
        for (size_t j = files_searching.size(); j < files_recv.size(); j++)
        {
            if (files_searching[i] == files_recv[j].file)
            {
                if (files_recv[i].unique_id == 0)
                {
                    files_recv[i].depth = files_recv[j].depth;
                    files_recv[i].id = files_recv[j].id;
                    files_recv[i].PORT = files_recv[j].PORT;
                    files_recv[i].unique_id = files_recv[j].unique_id;
                }
                else
                {
                    if (files_recv[i].depth > files_recv[j].depth)
                    {
                        files_recv[i].depth = files_recv[j].depth;
                    files_recv[i].id = files_recv[j].id;
                    files_recv[i].PORT = files_recv[j].PORT;
                    files_recv[i].unique_id = files_recv[j].unique_id;
                    }
                    else if (files_recv[i].depth == files_recv[j].depth)
                    {
                        if (files_recv[i].unique_id > files_recv[j].unique_id)
                        {
                            files_recv[i].depth = files_recv[j].depth;
                    files_recv[i].id = files_recv[j].id;
                    files_recv[i].PORT = files_recv[j].PORT;
                    files_recv[i].unique_id = files_recv[j].unique_id;
                        }
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < files_searching.size(); i++)
    {
        // Found bar.pdf at 4526 with MD5 0 at depth 1
        cout << "Found " << files_recv[i].file << " at " << files_recv[i].unique_id << " with MD5 0 at depth " << files_recv[i].depth << endl;
    }

    return 0;
}