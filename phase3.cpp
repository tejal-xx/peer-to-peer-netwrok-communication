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

struct neighbour
{
    int id;
    int PORT;
    int unique_private_id;
    int socket = 0;
    int socket_listen = 0;
};

struct files_recieved
{
    int unique_id;
    string file;
    int depth;
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

//file parameters

int offset = 0;
int ss = 0;
bool first_req = true;
int file_count = 0;

//

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
     ifstream infile;
    ofstream outfile;

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
    std::sort(files_searching.begin(), files_searching.end(), compareFunction);
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

    char buf[256]; // Buffer for client data

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
        const char *hello = s.c_str();
        // cout<<"blah"<<sizeof hello<<"blah"<<endl;
        send(pfds[i].fd, hello, s.size(), 0);
    }

    //////////////////////////////////////////////////////////////////
    // Main loop
    for (;;)
    {
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
                    int nbytes = recv(pfds[i].fd, buf, sizeof buf, 0);

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
                        if(buf[0] == '3' && buf[1] == '$'){

                            if(buf[2] == 'r'){ //requesting the files

                                string st = string(buf);
                                vector<string> rs;
                                tokenize(st,rs,"$");
                                string filename = rs[2];

                                streampos size;

                                infile.open(filename, ios::in|ios::binary|ios::ate);

                                if(infile.is_open()){
                                     size = infile.tellg();
                                    //memblock = new char [size];
                                    infile.seekg (0, ios::beg);
                                    
                                    ss = int(size);
                                }
                            }else if(buf[2] == 'e'){

                                
                            del_from_pfds(pfds, i, &fd_count);
                            total_listened1++;

                            }

                            if(infile.is_open()){
                                char* memblock1;
                                memblock1 = new char[1024];
                                memblock1[0] = '9';
                                memblock1[1] = '$';
                                memblock1[2] = 't';
                                memblock1[3] = '$';
                                if(ss-offset < 1000 ) {

                                    memblock1[2] = 'l';
                                    infile.read (&memblock1[4], ss-offset);
                                    
                                    
                                   // vread = read(new_socket, buffer, 1024);
                                    send(pfds[i].fd, memblock1, ss-offset, 0);
                                    
                                    break;}
                                else{
                                    

                                    infile.read (&memblock1[4], 1000);  //size
                                    
                                    //vread = read(new_socket, buffer, 1024);
                                    
                                    send(pfds[i].fd, memblock1, 1024, 0);
                                    
                                }
                                offset += 1000;

                            }
                           
                            

                        }

                        string s;
                        s.append(strtok(buf, "#"));
                        int x = 0;
                        vector<string> rslt;
                        tokenize(s, rslt, "@");
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
                            sending.append(to_string(unique_private_id) + "_");

                            for (size_t i = 0; i < recieved.size(); i++)
                            {
                                for (size_t j = 0; j < files.size(); j++)
                                {
                                    if (recieved[i] == files[j])
                                    {
                                        sending.append(recieved[i] + "_");
                                        break;
                                    }
                                }
                            }

                            sending.append("#");
                            const char *buf_temp = sending.c_str();
                            // cout<<buf_temp<<endl;
                            send(pfds[i].fd, buf_temp, sending.size(), 0);
                            // del_from_pfds(pfds, i, &fd_count);
                            break;
                        }
                        case 3:
                        {

                            del_from_pfds(pfds, i, &fd_count);
                            total_listened1++;
                        }
                        }
                    }
                }
                else
                {
                    int nbytes = recv(pfds[i].fd, buf, sizeof buf, 0);

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
                        if(buf[0] == '3' && buf[1] == '$'){

                            string sr = "3$t$";
                            const char* srr = sr.c_str();
                            outfile.write(&buf[4],1000);
		                    send(pfds[i].fd, srr, sizeof(srr), 0);

                            if(buf[2] == 'l'){
                                //last packet received

                                file_count += 1;

                                if(file_count<files_recv.size()){

                                    offset = 0;
                                    ss = 0;

                                    outfile.close();
                                    string req = "3$r$";
                                    string name = files_recv[file_count].file;  //have to initialize file count
                                    req.append(name);
                                    const char* req_buf = req.c_str();
                                    send(pfds[i].fd, req_buf, sizeof(req_buf), 0);
                                    outfile.open(name, ios::binary | ios::out);

                                }
                                else{
                                    string req = "3$e$";
                                    //string name = files_recv[file_count].file;  //have to initialize file count
                                    //req.append(name);
                                    const char* req_buf = req.c_str();
                                    send(pfds[i].fd, req_buf, sizeof(req_buf), 0);
                                    del_from_pfds(pfds, i, &fd_count);
                                    total_listened2++;
                                }
                            }

                            
                        }

                        string s;
                        s.append(strtok(buf, "#"));
                        int x = 0;
                        vector<string> rslt;
                        tokenize(s, rslt, "@");
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
                            // We got some good data from a client
                            string sending = "2@";
                            for (size_t i = 0; i < files_searching.size(); i++)
                            {
                                sending.append(files_searching[i] + "_");
                            }
                            sending.append("#");
                            const char *buf_temp = sending.c_str();
                            // cout<<buf_temp<<endl;
                            send(pfds[i].fd, buf_temp, sending.size(), 0);
                            // del_from_pfds(pfds, i, &fd_count);
                            break;
                        }
                        case 2:
                        {
                            vector<string> recieved;
                            tokenize(rslt[1], recieved, "_");
                            int unique = stoi(recieved[0]);
                            for (size_t i = 1; i < recieved.size(); i++)
                            {
                                for (size_t j = 0; j < files_recv.size(); j++)
                                {
                                    if (recieved[i] == files_recv[j].file)
                                    {
                                        if (unique < files_recv[j].unique_id || files_recv[j].unique_id == 0)
                                        {
                                            files_recv[j].unique_id = unique;
                                            files_recv[j].depth = 1;
                                            break;
                                        }
                                    }
                                }
                            }


                            //////////////////////
                            /*string sending = "3@END#";*/
                            //string sending = "9$r$";

                            //const char *buf_temp = sending.c_str();
                            //send(pfds[i].fd, buf_temp, sending.size(), 0);
                            //del_from_pfds(pfds, i, &fd_count);
                            //total_listened2++;
                            
                            string req = "3$r$";
                            string name = files_recv[file_count].file;  //have to initialize file count

                            
                            req.append(name);
                            if(files_recv[file_count].depth > 0){
                            const char* req_buf = req.c_str();
                            send(pfds[i].fd, req_buf, sizeof(req_buf), 0);
                            outfile.open(name, ios::binary | ios::out);
                            }
                            break;
                        }

                        // case 3 :
                        // {
                        //     // string req = "9$r$";
                        //     // string name = files_recv[file_count].file;
                        //     // req.append(name);
                        //     // const char* req_buf = req.c_str();
                        //     // send(pfds[i].fd, req_buf, req_buf.size(), 0);

                        //     //outfile.open(name, ios::binary | ios::out);
                        //     // string sr = "3$t$";
                        //     // const char* srr = sr.c_str();
                        //     // outfile.write(&buf[2],1000);
		                //     // send(pfds[i].fd, srr, srr.size(), 0);

                        // }

                        }

                    } // END handle data from client
                }     // END got ready-to-read from poll()
            }         // got ready-to-read from poll()
        }             // END looping through file descriptors
    }                 // END for(;;)--and you thought it would never end!
    for (size_t i = 0; i < files_recv.size(); i++)
    {
        // Found bar.pdf at 4526 with MD5 0 at depth 1
        cout << "Found " << files_recv[i].file << " at " << files_recv[i].unique_id << " with MD5 0 at depth " << files_recv[i].depth << endl;
    }

    return 0;
}