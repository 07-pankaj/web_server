#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/sendfile.h>

#define MAXLEN 10000

char error_404[] = "HTTP/1.1 404 Not Found\n";
char response_202[] = "HTTP/1.1 200 OK\r\n";
char content_type_text_html[] = "Content-Type: text/html\r\n";
char content_type_text_css[] = "Content-Type: text/css\r\n";
char content_type_image_jpg[] = "Content-Type: image/jpeg\r\n";
char content_type_image_gif[] = "Content-Type: image/gif\r\n";
char content_type_image_png[] = "Content-Type: image/png\r\n";
char content_length[] = "Content-Length:";

// use to store the request line

struct web_respones
{
    char method[8];
    char uri[1024];
    char version[128];
};

// coping string
void _strcpy(char *to, char *from)
{
    while ((*to++ = *from++) != '\0')
        ;
}

// checking and print the error message
//  socket create or not ,binding the socket lisenting
void _check(int val, char *message, int socket)
{
    if (val == -1)
    {
        perror(message);
        close(socket);
        exit(1);
    }
}

// geting request line from http request
void get_request_line(char *res, char *firstline)
{
    // request line end at \n(newline)
    while ((*firstline++ = *res++) != '\n')
        ;

    *firstline = '\0';
}

// HTTP prasing for http request
struct web_respones *parsing(char *respones)
{

    struct web_respones *wr = (struct web_respones *)malloc(sizeof(struct web_respones));
    char first_line[200] = {0};

    // seprating the request line from http request
    get_request_line(respones, first_line);

    // seprating the method ,uri and version from request line
    sscanf(first_line, "%s /%s %s", wr->method, wr->uri, wr->version);

    return wr;
}

// storing data into the file
void Save_data_to_file(char data[])
{

    FILE *s_file;

    // data store in file name http_data
    s_file = fopen("http_data", "w");

    // checking file open or not
    if (s_file == NULL)
    {
        printf("Failed to create file\n");
        printf("Data not saved \n");
    }
    else
    {
        // put data into the created file
        for (int i = 0; data[i] != '\0'; i++)
        {
            putc((int)data[i], s_file);
        }

        printf("data save successfull\n");
    }

    // closing file
    fclose(s_file);
}

void store_data(char *response, int size)
{
    char data[size]; // storing the  data which receives from http post request

    while (1)
    {
        // after \r\n\r\n our post data starts
        if (*response == '\r' && *(response + 1) == '\n' && *(response + 2) == '\r' && *(response + 3) == '\n')
        {
            response += 2;
            break;
        }
        response++;
    }

    int count = 0;
    while (1)
    {
        // post data send with http request  end with --/r/n
        if (*response == '-' && *(response + 1) == '-' && *(response + 2) == '\r' && *(response + 3) == '\n')
        {
            break;
        }
        // fetching the name provide by html from http request
        if (strncmp(response, "name=", 5) == 0)
        {
            response += 6;
            data[count++] = '\n';
            while ((*response != '"' && *(response + 1) != '\r' && *(response) != '\n') || (*response != '"' && *(response + 1) != ';'))
            {
                // printf("%c", *response);
                data[count++] = *response;

                response++;
            }
            // data[count++] = '\n';
        }
        else if (*response == '\r' && *(response + 1) == '\n' && *(response + 2) == '\r' && *(response + 3) == '\n')
        {

            // fetch the data from http request;
            while (1)
            {
                // data end at \r\n--
                if (*response == '\r' && *(response + 1) == '\n' && *(response + 2) == '-' && *(response + 3) == '-')
                {
                    break;
                }
                data[count] = *response;
                response++;
                count++;
            }

            data[count++] = '\n';
        }
        else
        {
            response++;
        }
    }

    // put null charater at the end of string(data)
    data[count] = '\n';
    data[++count] = '\0';
    Save_data_to_file(data);
}

//get the extension of image link :: png,jpeg,gif
void get_extension(char *filename, char *ext)
{
    char *start = strchr(filename, '.');
    start++;

    while (*ext++ = *start++)
    {
        ;
    }

    *ext = '\0';
}

void send_image_file(int client_socket, char *file_name)
{
    int ffpr;
    FILE *fpr;
    char buff[100];
    char extension[20];

    // geting extension of file name
    get_extension(file_name, extension);

    
    ffpr = open(file_name, O_RDONLY);

    fpr = fopen(file_name, "rb");
    int file_size = 100000;

    // help to find the size of the file
    if (fpr != NULL)
    {
        fseek(fpr, 0, SEEK_END);
        file_size = ftell(fpr);
        fseek(fpr, 0, SEEK_SET);
    }

    //sending image with proper extension to web browser
    if(strcmp(extension,"png") == 0)
    {
        sprintf(buff, "%s%s\r\n", response_202, content_type_image_png);
    }
    else if(strcmp(extension,"jpg") == 0)
    {
        sprintf(buff, "%s%s\r\n", response_202, content_type_image_jpg);
    }
    else if(strcmp(extension,"gif") == 0)
    {
        sprintf(buff, "%s%s\r\n", response_202, content_type_image_gif);
    }

  //      sprintf(buff, "%s%s\r\n", response_202, content_type_image_jpg);

    // sending 202 response to web browser
    send(client_socket, buff, strlen(buff), 0);

    // sendfile is use to copy data from one descriptor to another
    sendfile(client_socket, ffpr, NULL, file_size);

    // close file descriptor
    close(ffpr);

    // closing file
    fclose(fpr);
}

void get_web_res(char *send_buff, char *file_name, int client_socket)
{
    FILE *fpr;
    char get[200] = {0};
    int flag = 0;

    // concatenate the .html into the file name
    strcat(file_name, ".html");
    // opening file with is request by http request
    fpr = fopen(file_name, "r");

    if (fpr == NULL)
    {
        // file not found send 404 status
        strcpy(send_buff, error_404);
        perror("file not found");
        return;
    }
    else
    {

        // fseek(fpr, 0, SEEK_END);
        //  int res = ftell(fpr);
        // fseek(fpr, 0, SEEK_SET);
        // printf("entering  %d \n",res);

        // find the lenght of the request file above code also works to find length
        int file_length = 0;
        while (fgetc(fpr) != EOF)
        {
            file_length++;
        }

        // put file pointer at begining of the file
        rewind(fpr);

        // allocating the memory according to file size + 500 because of header data
        send_buff = (char *)realloc(send_buff, sizeof(char) * (file_length + 500));
        memset(send_buff, '\0', sizeof(send_buff));

        // concatenate the 200 status to send buffer
        sprintf(send_buff, "%s%s%s%d\r\n\r\n", response_202, content_type_text_html, content_length, file_length);

        // coping the request file data into the send buffer
        while (fgets(get, 200, (FILE *)fpr) != 0)
        {
            strcat(send_buff, get);
            memset(get, 0, strlen(get));
        }
    }

    // closing the file
    fclose(fpr);
}

// read http request ;
int read_http_request(int client_socket, char **client_input)
{

    char buffer[MAXLEN];
    int total_input_length = 0; // total lenght
    int rec_length;

    // converting socket from blocking to non-blocking;
    fcntl(client_socket, F_SETFL, O_NONBLOCK);
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        sleep(1);
        // reading data from sockets
        // if return value is -1  :: means we have read all data.
        if ((rec_length = recv(client_socket, buffer, MAXLEN, 0)) < 0)
        {
            // return total length of data receive from http browser
            return total_input_length;
        }
        else
        {
            total_input_length += rec_length + 1;
            // reallocate memory for buffer
            *client_input = (char *)realloc(*client_input, total_input_length * sizeof(char));
            strcat(*client_input, buffer);
        }
    }
}

void get_value_from_http_request(char *response, char *key, char *pair, int size)
{

    //parsing the value of header
    while (strncmp(response, "\r\n\r\n", 4) != 0)
    {
        //compare the key with response with size length bits
        if (strncmp(response, key, size) == 0)
        {
            response += (size + 2);

            while (strncmp(response, "\r\n", 2) != 0)
            {
                *pair++ = *response++;
            }
            *pair = '\0';

            break;
        }
        response++;
    }
}

// handle http request send by browser
void *handle_http_request(void *arg)
{

    printf("CONNECTION ESTABLISHED WITH CLIENT\n");

    // geting client_socket from arg is void pointer;
    int client_socket = *((int *)arg);
    free(arg);

    // used to store the  http request;
    char *buffer;

    // used to store response
    char *send_buffer;

    buffer = (char *)malloc(sizeof(char) * MAXLEN);
    memset(buffer, 0, sizeof(buffer));

    send_buffer = (char *)malloc(sizeof(char) * MAXLEN);
    memset(send_buffer, 0, sizeof(send_buffer));

    // struct which store the method ,uri,version;
    struct web_respones *web_resp;

    // fetch http request from sockets
    int recv_size = read_http_request(client_socket, &buffer);

    // for printing the http request send from browser uncomment the below line
    // printf("data received from http \n");
    // printf("%d\n", recv_size);
    // printf("------------------------------------------\n");
    // printf("%s\n %ld \n", buffer, strlen(buffer));
    // printf("-------------------------------------------\n");

    if (recv_size > 0)
    {

        // seprating the the request line  from http request
        web_resp = parsing(buffer); // geting uri , method and version

        // http request method is POST
        if (strcmp(web_resp->method, "POST") == 0)
        {
            // store the body(data send using post request) of http request
            store_data(buffer, recv_size);

            // geting the web response for request line
            get_web_res(send_buffer, web_resp->uri, client_socket);

            // send the response to web browser
            send(client_socket, send_buffer, strlen(send_buffer), 0);
        }
        else if ((strcmp(web_resp->method, "GET") == 0) && strcmp(web_resp->uri, "favicon.ico") != 0) // http request method is GET
        {
            char value[200];

            // geting the value of Sec-Fetch-Dest it will tell weather user need image or document
            get_value_from_http_request(buffer, "Sec-Fetch-Dest", value, 14);

            if (strcmp(value, "image") == 0)
            {
                // sending image file to web_browser
                send_image_file(client_socket, web_resp->uri);
            }
            else if ((strcmp(value, "document") == 0) || (strcmp(value, "style") == 0))
            {
                // geting the web response for request line
                get_web_res(send_buffer, web_resp->uri, client_socket);

                // send the response to web browser
                send(client_socket, send_buffer, strlen(send_buffer), 0);
            }
        }

        printf("http respsonse send\n\n");

        free(web_resp);
    }

    // free allocate memory
    free(buffer);
    free(send_buffer);

    // closing socket
    close(client_socket);

    printf("DISCONNECTED\n\n");
    return NULL;
}

// listening all http request
void listening_client(int server_socket)
{
    struct sockaddr_storage client_addr;
    socklen_t address_size;
    int client_socket;
    while (1)
    {
        address_size = sizeof(client_addr);

        // accepting socket connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &address_size);

        // checking  socket is connected or not
        _check(client_socket, "FAILED TO CONNECTED", client_socket);

        pthread_t request_thread;

        int *ptr = (int *)malloc(sizeof(int));
        *ptr = client_socket;

        // creating thread for each http request
        pthread_create(&request_thread, NULL, handle_http_request, (void *)ptr);
    }
}

int main()
{

    int server_socket;
    struct addrinfo server_add, *res;

    memset(&server_add, 0, sizeof server_add);
    server_add.ai_family = AF_UNSPEC;     // use IPv4 or IPv6, whichever
    server_add.ai_socktype = SOCK_STREAM; // TCP
    server_add.ai_flags = AI_PASSIVE;     // fill in my IP for me

    getaddrinfo(NULL, "8080", &server_add, &res);

    // socket(domain,type,protocol)
    server_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    int yes = 1;

    _check(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes), "setsockopt", server_socket);

    // check socket create or not
    _check(server_socket, "FAILED TO CREATE SOCKET", server_socket);

    printf("Socket created \n\n");

    // bind the socket
    _check(bind(server_socket, res->ai_addr, res->ai_addrlen), "FAILED TO BIND", server_socket); // check socket binding

    // int listen(int socket, int backlog);
    _check(listen(server_socket, 10), "FAILED TO LISTEN", server_socket); // check listening

    printf("Listening......\n");

    // listening all http request
    listening_client(server_socket);

    // closing the server socket
    close(server_socket);
    return 0;
}
