
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dbus/dbus.h>



/*
 * listen, wait a call or a signal
 */
#define DBUS_SENDER_BUS_NAME        "cn.zeke.sender_app"

#define DBUS_RECEIVER_BUS_NAME      "cn.zeke.receiver_app"
#define DBUS_RECEIVER_PATH          "/cn/zeke/object"
#define DBUS_RECEIVER_INTERFACE     "cn.zeke.interface"
#define DBUS_RECEIVER_SIGNAL        "signal"
#define DBUS_RECEIVER_METHOD        "method"

#define DBUS_RECEIVER_SIGNAL_RULE   "type='signal',interface='%s'"
#define DBUS_RECEIVER_REPLY_STR     "i am %d, get a message"

#define MODE_SIGNAL                 1
#define MODE_METHOD                 2

#define DBUS_CLIENT_PID_FILE        "/tmp/dbus-client.pid"

/**
 *
 * @param msg
 * @param conn
 */
void reply_method_call(DBusMessage *msg, DBusConnection *conn) {
    DBusMessage *reply;
    DBusMessageIter reply_arg;
    DBusMessageIter msg_arg;
    dbus_uint32_t serial = 0;

    pid_t pid;
    char reply_str[128];
    void *__value;
    char *__value_str;
    int __value_int;

    int ret;

    pid = getpid();

    //创建返回消息reply
    reply = dbus_message_new_method_return(msg);
    if (!reply) {
        printf("Out of Memory!\n");
        return;
    }

    //在返回消息中填入参数。
    snprintf(reply_str, sizeof(reply_str), DBUS_RECEIVER_REPLY_STR, pid);
    __value_str = reply_str;
    __value = &__value_str;

    dbus_message_iter_init_append(reply, &reply_arg);
    if (!dbus_message_iter_append_basic(&reply_arg, DBUS_TYPE_STRING, __value)) {
        printf("Out of Memory!\n");
        goto out;
    }

    //从msg中读取参数，根据传入参数增加返回参数
    if (!dbus_message_iter_init(msg, &msg_arg)) {
        printf("Message has NO Argument\n");
        goto out;
    }

    do {
        int ret = dbus_message_iter_get_arg_type(&msg_arg);
        if (DBUS_TYPE_STRING == ret) {
            dbus_message_iter_get_basic(&msg_arg, &__value_str);
            printf("I am %d, get Method Argument STRING: %s\n", pid,
                   __value_str);

            __value = &__value_str;
            if (!dbus_message_iter_append_basic(&reply_arg,
                                                DBUS_TYPE_STRING, __value)) {
                printf("Out of Memory!\n");
                goto out;
            }
        } else if (DBUS_TYPE_INT32 == ret) {
            dbus_message_iter_get_basic(&msg_arg, &__value_int);
            printf("I am %d, get Method Argument INT32: %d\n", pid,
                   __value_int);

            __value_int++;
            __value = &__value_int;
            if (!dbus_message_iter_append_basic(&reply_arg,
                                                DBUS_TYPE_INT32, __value)) {
                printf("Out of Memory!\n");
                goto out;
            }
        } else {
            printf("Argument Type ERROR\n");
        }

    } while (dbus_message_iter_next(&msg_arg));

    //发送返回消息
    if (!dbus_connection_send(conn, reply, &serial)) {
        printf("Out of Memory\n");
        goto out;
    }

    dbus_connection_flush(conn);
    out:
    dbus_message_unref(reply);
}

/* 监听D-Bus消息，我们在上次的例子中进行修改 */
void dbus_receive(void) {
    DBusMessage *msg;
    DBusMessageIter arg;
    DBusConnection *connection;
    DBusError err;

    pid_t pid;
    char name[64];
    char rule[128];

    const char *path;
    void *__value;
    char *__value_str;
    int __value_int;

    int ret;

    pid = getpid();

    dbus_error_init(&err);
    //创建于session D-Bus的连接
    connection = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (!connection) {
        if (dbus_error_is_set(&err))
            printf("Connection Error %s\n", err.message);

        goto out;
    }

    //设置一个BUS name
    if (0 == access(DBUS_CLIENT_PID_FILE, F_OK))
        snprintf(name, sizeof(name), "%s%d", DBUS_RECEIVER_BUS_NAME, pid);
    else
        snprintf(name, sizeof(name), "%s", DBUS_RECEIVER_BUS_NAME);

    printf("i am a receiver, PID = %d, name = %s\n", pid, name);

    ret = dbus_bus_request_name(connection, name,
                                DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        if (dbus_error_is_set(&err))
            printf("Name Error %s\n", err.message);

        goto out;
    }

    //要求监听某个signal：来自接口test.signal.Type的信号
    snprintf(rule, sizeof(rule), DBUS_RECEIVER_SIGNAL_RULE, DBUS_RECEIVER_INTERFACE);
    dbus_bus_add_match(connection, rule, &err);
    dbus_connection_flush(connection);
    if (dbus_error_is_set(&err)) {
        printf("Match Error %s\n", err.message);
        goto out;
    }

    while (1) {
        dbus_connection_read_write(connection, 0);

        msg = dbus_connection_pop_message(connection);
        if (msg == NULL) {
            sleep(1);
            continue;
        }

        path = dbus_message_get_path(msg);
        if (strcmp(path, DBUS_RECEIVER_PATH)) {
            printf("Wrong PATH: %s\n", path);
//            goto next;
        }

        printf("Get a Message\n");
        if (!dbus_message_iter_init(msg, &arg)) {
            break;
        }
        char *sig = dbus_message_iter_get_signature(&arg);
        int type = dbus_message_iter_get_arg_type(&arg);
        printf("sig %s type %d \n", sig, type);
        if (type == 114) {
            DBusMessageIter structIter;
            dbus_message_iter_recurse(&arg, &structIter);
            char *svalue;
            int ivalue;
            do {
                sig = dbus_message_iter_get_signature(&structIter);
                type = dbus_message_iter_get_arg_type(&structIter);
                printf("sig %s type %d \n", sig, type);
                if (type == DBUS_TYPE_STRING) {
                    dbus_message_iter_get_basic(&structIter, &svalue);
                    printf("value type string -->  %s \n", svalue);
                } else if (type == DBUS_TYPE_INT32) {
                    dbus_message_iter_get_basic(&structIter, &ivalue);
                    printf("value type int32 --> %d \n", ivalue);
                } else if (type == DBUS_TYPE_ARRAY) {
                    DBusMessageIter arrayIter;
                    dbus_message_iter_recurse(&structIter, &arrayIter);
                    int count = dbus_message_iter_get_element_count(&structIter);
                    int eType = dbus_message_iter_get_element_type(&structIter);
                    int32_t *array;
                    dbus_message_iter_get_fixed_array(&arrayIter, &array, &count);
                    int i;
                    for (i = 0; i < count; ++i) {
                        printf("array[%d] %d \n", i, array[i]);
                    }
                }
            } while (dbus_message_iter_next(&structIter));
        }
        dbus_message_unref(msg);
    }

    out:
    dbus_error_free(&err);
}

/*
 * call a method
 */
static void dbus_send(int mode, char *type, void *value) {
    DBusConnection *connection;
    DBusError err;
    DBusMessage *msg;
    DBusMessageIter arg;
    DBusPendingCall *pending;
//    dbus_uint32_t serial;

//    int __type;
//    void *__value;
//    char *__value_str;
//    int __value_int;
//    pid_t pid;
    int ret;

//    pid = getpid();

    //Step 1: connecting session bus
    /* initialise the erroes */
    dbus_error_init(&err);

    /* Connect to Bus*/
    connection = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (!connection) {
        if (dbus_error_is_set(&err))
            printf("Connection Err : %s\n", err.message);

        goto out1;
    }

    //step 2: 设置BUS name，也即连接的名字。
    ret = dbus_bus_request_name(connection, DBUS_SENDER_BUS_NAME,
                                DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        if (dbus_error_is_set(&err))
            printf("Name Err : %s\n", err.message);

        goto out1;
    }


//    if (!strcasecmp(type, "STRING")) {
//        __type = DBUS_TYPE_STRING;
//        __value_str = value;
//        __value = &__value_str;
//    } else if (!strcasecmp(type, "INT32")) {
//        __type = DBUS_TYPE_INT32;
//        __value_int = atoi(value);
//        __value = &__value_int;
//    } else {
//        printf("Wrong Argument Type\n");
//        goto out1;
//    }


//    printf("Call app[bus_name]=%s, object[path]=%s, interface=%s, method=%s\n",
//           DBUS_RECEIVER_BUS_NAME, DBUS_RECEIVER_PATH,
//           DBUS_RECEIVER_INTERFACE, DBUS_RECEIVER_METHOD);

    //针对目的地地址，创建一个method call消息。
    //Constructs a new message to invoke a method on a remote object.
    msg = dbus_message_new_method_call(
            DBUS_RECEIVER_BUS_NAME, DBUS_RECEIVER_PATH,
            DBUS_RECEIVER_INTERFACE, DBUS_RECEIVER_METHOD);
    if (msg == NULL) {
        printf("Message NULL");
        goto out1;
    }

    dbus_message_iter_init_append(msg, &arg);
    DBusMessageIter structIter;
    printf("start to open container \n");
    dbus_message_iter_open_container(&arg, DBUS_TYPE_STRUCT, NULL, &structIter);
    printf("open container 0 done \n");
    char *strMsg = "Hell I'am Jack";

    printf("try to add string to iter \n");
    dbus_message_iter_append_basic(&structIter, DBUS_TYPE_STRING, &strMsg);
    printf("try to add string to iter done \n");
    int id = 111000;
    printf("try to  append int \n");
    dbus_message_iter_append_basic(&structIter, DBUS_TYPE_INT32, &id);
    printf("append for container 0 done \n");
    DBusMessageIter arrayIter;
    char buf[2];
    buf[0] = DBUS_TYPE_INT32;
    buf[1] = '\0';
    int eCount = 10;
    int *array = malloc(eCount * sizeof(int32_t));
    int i;
    for (i = 0; i < eCount; ++i) {
        array[i] = 10 - i;
        printf("array[%d] %d ", i, array[i]);
    }
    printf("\ntry to open container 1\n");
    dbus_message_iter_open_container(&structIter, DBUS_TYPE_ARRAY, buf, &arrayIter);
    printf("open container 1 done \n");
    if (!dbus_message_iter_append_fixed_array(&arrayIter, DBUS_TYPE_INT32, &array, eCount)) {
        printf("err--> %s", err.message);
    }

    printf("add array done\n");
    dbus_message_iter_close_container(&structIter, &arrayIter);
    dbus_message_iter_close_container(&arg, &structIter);
    printf("close done");
    free(array);

//        if (!dbus_message_iter_append_basic(&arg, __type, __value)) {
//            printf("Out of Memory!");
//            goto out2;
//        }

    //发送消息并获得reply的handle 。Queues a message to send, as with dbus_connection_send() , but also returns a DBusPendingCall used to receive a reply to the message.
    if (!dbus_connection_send_with_reply(connection, msg, &pending, -1)) {
        printf("Out of Memory!");
        goto out2;
    }

    if (pending == NULL) {
        printf("Pending Call NULL: connection is disconnected ");
        goto out2;
    }

    dbus_connection_flush(connection);

    out2:
    dbus_message_unref(msg);
    out1:
    dbus_error_free(&err);
}

static void usage(void) {
#define USAGE "usage: ./dbus-client [send | receive] <param>\n" \
    "\treceive -- listen, wait a signal or a method call\n" \
    "\t\tif you want to test signal broadcast, run two receiver like this:\n" \
    "\t\trm -f /tmp/dbus-client.pid\n" \
    "\t\t./dbus-client receive &\n" \
    "\t\techo > /tmp/dbus-client.pid\n" \
    "\t\t./dbus-client receive &\n" \
    "\tsend [mode] [type] [value] -- send a signal or call a method\n" \
    "\t\tmode -- SIGNAL | METHOD\n" \
    "\t\ttype -- STRING | INT32\n" \
    "\t\tvalue -- string or number\n" \
    "\t\texample:\n" \
    "\t\t./dbus-client send SIGNAL STRING hello\n" \
    "\t\t./dbus-client send METHOD INT32 99\n" \
    "\n"
    printf(USAGE);
}
/*
int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage();
        return -1;
    }

    if (!strcmp(argv[1], "receive")) {
        dbus_receive();
    } else if (!strcmp(argv[1], "send")) {
        if (argc < 5) {
            usage();
        } else {
            if (!strcasecmp(argv[2], "SIGNAL"))
                dbus_send(MODE_SIGNAL, argv[3], argv[4]);
            else if (!strcasecmp(argv[2], "METHOD"))
                dbus_send(MODE_METHOD, argv[3], argv[4]);
            else
                usage();
        }
    } else {
        usage();
    }

    return 0;
}*/


#define BLUEZ_BUS_NAME "org.bluez"
#define OBJECT_MAGAGER_INTERFACE "org.freedesktop.DBus.ObjectManager"

int handle_dbus_error(DBusError *err, const char *func, int line) {
    if (dbus_error_is_set(err)) {
        fprintf(stderr, "DBus error %s at %u: %s\n", func, line, err->message);
        dbus_error_free(err);
        return 1;
    }
    return 0;
}

int get_system_dbus(DBusConnection **connection) {
    DBusError err;
    DBusConnection *conn;
    dbus_error_init(&err);
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    handle_dbus_error(&err, __FUNCTION__, __LINE__);
    if (conn == NULL)return 0;
    *connection = conn;
    printf("Dbus Name %s \n", dbus_bus_get_unique_name(conn));
    return 1;
}

int get_bluez_api(DBusConnection *connection) {
    DBusMessage *msg, *reply;
    DBusMessageIter iter;
    DBusError err;
    char *method = "GetManagedObjects";

    dbus_error_init(&err);

    msg = dbus_message_new_method_call(BLUEZ_BUS_NAME, "/", OBJECT_MAGAGER_INTERFACE, method);

    reply = dbus_connection_send_with_reply_and_block(connection, msg, -1, &err);
    handle_dbus_error(&err, __FUNCTION__, __LINE__);
    if (!reply) {
        printf("Reply is NULL");
        return 0;
    }
    //read return value
    if (!dbus_message_iter_init(reply, &iter)) {
        printf("Get return value failed \n");
    } else {
        printf("access return value start -------------\n");
        char *sig = dbus_message_iter_get_signature(&iter);
        printf("signature %s \n", sig);
        int type = dbus_message_iter_get_arg_type(&iter);
        printf("return type -->  %c\n", type);
        if (type == DBUS_TYPE_ARRAY) {
            int count = dbus_message_iter_get_element_count(&iter);
            int eleType = dbus_message_iter_get_element_type(&iter);
            printf("eleCount %d eleType %c \n", count, eleType);
            DBusMessageIter arrayIter;
            dbus_message_iter_recurse(&iter, &arrayIter);
            do {
//                DBusMessageIter arrayEle;
//                dbus_message_iter_recurse(&arrayIter, &arrayEle);
                sig = dbus_message_iter_get_signature(&arrayIter);
                type = dbus_message_iter_get_arg_type(&arrayIter);
                printf("  array --> sig %s type %d \n", sig, type);
                DBusMessageIter dictIter;
                dbus_message_iter_recurse(&arrayIter, &dictIter);
                do {
                    sig = dbus_message_iter_get_signature(&dictIter);
                    type = dbus_message_iter_get_arg_type(&dictIter);
                    printf("    dict --> sig %s type %d \n", sig, type);
                    if (type == DBUS_TYPE_OBJECT_PATH) {
                        char *path;
                        dbus_message_iter_get_basic(&dictIter, &path);
                        printf("    path --> %s\n", path);
                    } else if (type == DBUS_TYPE_ARRAY) {
                        DBusMessageIter nextArrayIter;
                        dbus_message_iter_recurse(&dictIter, &nextArrayIter);
                        DBusMessageIter eIter;

                        do {
                            dbus_message_iter_recurse(&nextArrayIter, &eIter);
                            do {
                                sig = dbus_message_iter_get_signature(&eIter);
                                type = dbus_message_iter_get_arg_type(&eIter);
                                printf("      nextArray --> sig %s type %d \n", sig, type);
                                if (type == DBUS_TYPE_STRING) {
                                    char *sv;
                                    dbus_message_iter_get_basic(&eIter, &sv);
                                    printf("       sv %s\n", sv);
                                } else if (type == DBUS_TYPE_ARRAY) {
                                    DBusMessageIter nnIter;
                                    dbus_message_iter_recurse(&eIter, &nnIter);
                                    do {
                                        sig = dbus_message_iter_get_signature(&nnIter);
                                        type = dbus_message_iter_get_arg_type(&nnIter);
                                        printf("        nnIter--> sig %s type %c \n", sig, type);
                                        if (type == DBUS_TYPE_DICT_ENTRY) {
                                            DBusMessageIter nnDictIter;
                                            dbus_message_iter_recurse(&nnIter, &nnDictIter);
                                            do {
                                                sig = dbus_message_iter_get_signature(&nnDictIter);
                                                type = dbus_message_iter_get_arg_type(&nnDictIter);
                                                printf("          nnIter--> sig %s type %c \n", sig, type);
                                                if (type == DBUS_TYPE_STRING) {
                                                    char *sv;
                                                    dbus_message_iter_get_basic(&nnDictIter, &sv);
                                                    printf("          sv %s\n", sv);
                                                } else {
                                                    DBusMessageIter vIter;
                                                    dbus_message_iter_recurse(&nnDictIter, &vIter);
                                                    do {
                                                        sig = dbus_message_iter_get_signature(&nnDictIter);
                                                        type = dbus_message_iter_get_arg_type(&nnDictIter);
                                                        printf("            vIter--> sig %s type %c \n", sig, type);
                                                    } while (dbus_message_iter_next(&vIter));
                                                }
                                            } while (dbus_message_iter_next(&nnDictIter));
                                        }
                                    } while (dbus_message_iter_next(&nnIter));
                                }
                            } while (dbus_message_iter_next(&eIter));
                        } while (dbus_message_iter_next(&nextArrayIter));
                    }
                } while (dbus_message_iter_next(&dictIter));
            } while (dbus_message_iter_next(&arrayIter));
        }
        printf("access return value end -------------\n");
//        char *value=calloc(1024,sizeof(char));
        dbus_message_unref(reply);
    }


}

void get_properties(DBusConnection *conn, char *interface, char *name) {
    DBusMessage *msg, *reply;
    DBusError err;
    DBusMessageIter argv, value;
    dbus_error_init(&err);

    msg = dbus_message_new_method_call(BLUEZ_BUS_NAME, "/org/bluez/hci0", "org.freedesktop.DBus.Properties", "Get");
    dbus_message_iter_init_append(msg, &argv);
    dbus_message_iter_append_basic(&argv, DBUS_TYPE_STRING, &interface);
    dbus_message_iter_append_basic(&argv, DBUS_TYPE_STRING, &name);
    reply = dbus_connection_send_with_reply_and_block(conn, msg, 100, &err);
    if (reply == NULL) {
        printf("NULL Get --> %s ", err.message);
    }
    int type;

    dbus_message_iter_init(reply, &value);
    type = dbus_message_iter_get_arg_type(&value);
    if (type == DBUS_TYPE_VARIANT) {
        DBusMessageIter vari;
        dbus_message_iter_recurse(&value, &vari);
        do {
            type = dbus_message_iter_get_arg_type(&vari);
            char *vs;
            int vi;
            char vb;
            unsigned int vui;
            switch (type) {
                case DBUS_TYPE_STRING:
                    dbus_message_iter_get_basic(&vari, &vs);
                    printf("%s --> %s \n", name, vs);
                    break;
                case DBUS_TYPE_INT32:
                    dbus_message_iter_get_basic(&vari, &vi);
                    printf("%s --> %d \n", name, vi);
                    break;
                case DBUS_TYPE_BOOLEAN:
                    dbus_message_iter_get_basic(&vari, &vb);
                    printf("%s --> %s \n", name, vb ? "TRUE" : "FALSE");
                    break;
                case DBUS_TYPE_UINT32:
                    dbus_message_iter_get_basic(&vari, &vui);
                    printf("%s --> %d \n", name, vui);
                    break;
                default:
                    printf("Name %s Type -->  %c unsupport \n", name, type);
            }
        } while (dbus_message_iter_next(&vari));
    }
    if (reply != NULL) {
        dbus_message_unref(reply);
    }
    dbus_message_unref(msg);
    dbus_error_free(&err);
}

void set_property(DBusConnection *conn, char *interface, char *name, void *value, char *type) {
    DBusMessage *msg, *reply;
    DBusError err;
    DBusMessageIter argv, v;
    dbus_error_init(&err);

    msg = dbus_message_new_method_call(BLUEZ_BUS_NAME, "/org/bluez/hci0", "org.freedesktop.DBus.Properties", "Set");
    dbus_message_iter_init_append(msg, &argv);
    dbus_message_iter_append_basic(&argv, DBUS_TYPE_STRING, &interface);
    dbus_message_iter_append_basic(&argv, DBUS_TYPE_STRING, &name);
    dbus_message_iter_open_container(&argv, DBUS_TYPE_VARIANT, type, &v);
    dbus_message_iter_append_basic(&v, type[0], value);
    dbus_message_iter_close_container(&argv, &v);
    reply = dbus_connection_send_with_reply_and_block(conn, msg, 100, &err);
    if (reply == NULL) {
        printf("reply is null %s \n", err.message);
    } else {
        dbus_message_unref(reply);
    }

    dbus_message_unref(msg);

}

void set_name(DBusConnection *connection, char *newName) {
    set_property(connection, "org.bluez.Adapter1", "Alias", &newName, "s");
}

void set_discoverable_time_out(DBusConnection *connection, unsigned int to) {
    set_property(connection, "org.bluez.Adapter1", "DiscoverableTimeout", &to, "u");
}

void set_filter(DBusConnection *conn) {
    char *objectPath = "/org/bluez/hci0";
    char *interface = "org.bluez.Adapter1";
    char *method = "SetDiscoveryFilter";

    DBusMessage *msg, *reply;
    DBusError err;

    dbus_error_init(&err);

    msg = dbus_message_new_method_call(BLUEZ_BUS_NAME, objectPath, interface, method);
    DBusMessageIter argv;
    dbus_message_iter_init_append(msg, &argv);
    DBusMessageIter v;
    dbus_message_iter_open_container(&argv, DBUS_TYPE_DICT_ENTRY, NULL, &v);
    dbus_message_iter_close_container(&argv, &v);
    reply = dbus_connection_send_with_reply_and_block(conn, msg, 100, &err);
    if (reply == NULL) {
        printf("Filter--> reply is null . no thing get ? err-msg -->  %s \n", err.message);
    } else {
        DBusMessageIter iter;
        dbus_message_iter_init(msg, &iter);
        char *sig = dbus_message_iter_get_signature(&iter);
        printf("sig--> %s err --> %s \n", sig, err.message);
    }


    dbus_error_free(&err);
}

void start_discovery(DBusConnection *connection) {
    char *objectPath = "/org/bluez/hci0";
    char *interface = "org.bluez.Adapter1";
    char *method = "StartDiscovery";

    DBusMessage *msg, *reply;
    DBusError err;

    dbus_error_init(&err);

    msg = dbus_message_new_method_call(BLUEZ_BUS_NAME, objectPath, interface, method);
    reply = dbus_connection_send_with_reply_and_block(connection, msg, 100, &err);
    if (reply == NULL) {
        printf("reply is null . no thing get ? err-msg -->  %s \n", err.message);
    } else {
        DBusMessageIter iter;
        dbus_message_iter_init(msg, &iter);
        char *sig = dbus_message_iter_get_signature(&iter);
        printf("sig--> %s err --> %s \n", sig, err.message);
    }


    dbus_error_free(&err);
}

int main() {
    DBusConnection *systemConn;
    if (get_system_dbus(&systemConn)) {
//        get_bluez_api(systemConn);
//        set_name(systemConn, "Bluez-x1000");
        start_discovery(systemConn);
//        set_discoverable_time_out(systemConn, 0);
        get_properties(systemConn, "org.bluez.Adapter1", "Name");
        get_properties(systemConn, "org.bluez.Adapter1", "Address");
        get_properties(systemConn, "org.bluez.Adapter1", "Alias");
        get_properties(systemConn, "org.bluez.Adapter1", "Powered");
        get_properties(systemConn, "org.bluez.Adapter1", "Discoverable");
        get_properties(systemConn, "org.bluez.Adapter1", "Pairable");
        get_properties(systemConn, "org.bluez.Adapter1", "PairableTimeout");
        get_properties(systemConn, "org.bluez.Adapter1", "DiscoverableTimeout");
        get_properties(systemConn, "org.bluez.Adapter1", "Discovering");


        dbus_connection_unref(systemConn);
    }
}


