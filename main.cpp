#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <thread>
#include <random>

#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <cstring>


using namespace std;

static map<string, map<size_t, size_t>> data;
static streamoff pos {0};
static bool f_run = true;


void createData ()
{
    cout << "Create data ..." << endl;
    const string name_file ("test.dat");
    static vector<string>  events {"ORDER", "TRANS", "BUY"};

    ofstream fout( name_file );
    if (fout.is_open()) {
        time_t raw_time;
        time( &raw_time );
        struct tm *time_info;
        time_info = localtime( &raw_time );

        char str_time [30];
        strftime (str_time, 30, "[%d-%m-%y %H:%M:%S]", time_info);
        fout << str_time << " Statistics gathering started" << endl;
        fout << "TIME        "    << "\t" << "EVENT" << "\t" << "CALLCNT" << "\t" << "FILLCNT"
             << "\t" << "AVGSIZE" << "\t" << "MAXSIZE" << "\t" << "AVGFULL" << "\t" << "MAXFULL"
             << "\t" << "MINFULL" << "\t" << "AVGDLL"  << "\t" << "MAXDLL"  << "\t" << "AVGTRIP"
             << "\t" << "MAXTRIP" << "\t" << "AVGTEAP" << "\t" << "MAXTEP"  << "\t" << "AVGTSMR" << endl;


        const int nm_rolls = 1'000'000;
        std::default_random_engine generator;
        std::chi_squared_distribution<double> distribution(5);
        constexpr size_t num {36};

        size_t cnt {0};
        do {
            time_t raw_time;
            time( &raw_time );
            struct tm *time_info;
            time_info = localtime( &raw_time );

            char str_time [15];
            strftime (str_time, 15, "[%H:%M:%S]", time_info);

            if (cnt%(nm_rolls/100) == 0){
                cout << str_time << "   generated " << (cnt/(nm_rolls/100))+1 << "%" << endl;
            }

            auto callcnt {0};
            auto avgtsmr {(rand() % 9) + 110};
            double number = distribution( generator );
            if ((number >= 0.0) && ( number <= num))
                avgtsmr = int(number)+110;
            for (decltype(events.size()) i {0}; i < events.size(); ++i){
                fout << str_time << "\t" << events[i];
                for (auto i {0}; i < 13; ++i) {
                    fout << "\t" << callcnt;
                }
                fout << "\t" << avgtsmr << endl;
            }
        }
        while (++cnt < nm_rolls);

        fout.close();
    }
    else {
        cout << "File " << name_file << " is not open!";
    }
}

void loadData (const string pathFile)
{
    ifstream ffile( pathFile );
    if (!pathFile.empty()){
        if (!ffile.is_open()) {
            cerr << "LogFile " << pathFile << "is not open!" << endl;
            exit(1);
        }

        ffile.seekg(0, std::ios::end);
        if (ffile.tellg() <= pos){
            ffile.close();
            return;
        }

        ffile.seekg (pos);
    }
    istream& istrm = pathFile.empty() ? cin : ffile;

    string str;
    if (!pathFile.empty()){
        if (pos){
            istrm.seekg (pos);
        }
        else {
            getline(istrm, str);
            getline(istrm, str);
        }
    }
    else {
        getline(istrm, str);
        getline(istrm, str);
    }

    string time;
    string event;
    string callcnt;
    size_t avgtsmr;


    while (getline(istrm, str) && f_run)
    {
        if(!pathFile.empty())
            pos = istrm.tellg();
        istringstream iss( str );
        iss >> time >> event;
        for (auto i {0}; i < 13; ++i) {
            iss >> callcnt;
        }
        iss >> avgtsmr;

        data[event][avgtsmr] += 1;
    }

    if (!pathFile.empty())
        ffile.close();
}

void loadDataFromFIFO(const string namePIPE)
{
    ifstream fifo( namePIPE );
    if (fifo.is_open()){
        string str;
        string time;
        string event;
        string callcnt;
        size_t avgtsmr;
        getline(fifo, str);
        getline(fifo, str);

        while (f_run){
            while (getline(fifo, str))
            {
                istringstream iss( str );
                iss >> time >> event;
                for (auto i {0}; i < 13; ++i) {
                    iss >> callcnt;
                }
                iss >> avgtsmr;

                data[event][avgtsmr] += 1;
            }
            if (fifo.eof())
                fifo.clear();
            else {
                break;
            }
        }

        fifo.close();
    }
    remove( namePIPE.c_str() );
}

auto getStatisticsEvent(string nameEvent)
{
    auto one_percent {0.0};
    auto iter_event = data.find( nameEvent );
    if (iter_event != data.end())
    {
        auto dataEvent  {data[nameEvent]};
        size_t v_min    {dataEvent.cbegin()->first};
        size_t v_50     {0};
        size_t v_90     {0};
        size_t v_99     {0};
        size_t v_100    {0};


        size_t cnt {0};
        for (auto it {dataEvent.cbegin()}; it != dataEvent.cend(); ++it){
            cnt += it->second;
        }
        one_percent = cnt/100.;


        auto percent    {0.0};
        for (auto it {dataEvent.cbegin()}; it != dataEvent.cend(); ++it){
            auto weight = it->second/one_percent;
            percent += weight;

            if (percent < 50.0){
                v_50 = it->first + 1;
                continue;
            }
            else {
                if (percent < 90.0){
                    v_90 = it->first + 1;
                    continue;
                }
                else {
                    if (percent < 99.0){
                        v_99 = it->first + 1;
                        continue;
                    }
                    else {
                        if (percent >= 99.9) {
                            v_100 = it->first;
                            break;
                        }
                    }
                }
            }
        }
        cout << endl << nameEvent << " min=" << v_min << " 50%=" << v_50
             << " 90%=" << v_90 << " 99%=" << v_99 << " 99.9%=" << v_100 << endl;
    }


    return one_percent;
}

void getFullStatistics()
{
    for (const auto &dataEvent: data){
        auto one_percent = getStatisticsEvent(dataEvent.first);

        cout << "ExecTime" << "\tTransNo" << "\tWeight" << "\tPercent" << endl;
        auto percent = 0.0;
        size_t cnt_trans {0};

        auto &mpEvent = dataEvent.second;
        for (auto i {mpEvent.cbegin()->first}; i <= mpEvent.crbegin()->first; ++i) {
            auto iter = mpEvent.find(i);
            if (iter != mpEvent.cend())
                cnt_trans += iter->second;

            if (i % 5 == 0){
                auto weight = trunc((cnt_trans/one_percent)*100)/100;
                percent += weight;
                cout << i << "\t\t" << cnt_trans << "\t" << weight << "\t" << percent << endl;

                cnt_trans = 0;
            }
        }
    }
}

void hdlSIGUSR1(int)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &set, nullptr);
    getFullStatistics();
    sigprocmask(SIG_UNBLOCK, &set, nullptr);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        cout << "Usage:" << endl;
        cout << "\t./name_app -p [port] -i [file] -o [file] -f [fifo]" << endl;
        cout << "\t-i [file] input file (logfile) | optional parametr" << endl;
        cout << "\t-o [file] output file          | optional parametr" << endl;
        cout << "\t-f [fifo] named pipe           | optional parametr" << endl;

        return 0;
    }

    string opts = "p:i:o:f:";
    uint16_t port {0};
    string ifile;
    string ofile;
    string fifo;

    int opt;
    while((opt = getopt(argc, argv, opts.c_str())) != -1) {
        switch(opt) {
        case 'p':
            port = static_cast<uint16_t>(atoi(optarg));
            break;
        case 'i':
            ifile = optarg;
            break;
        case 'o':
            ofile = optarg;
            freopen (optarg, "w", stdout);
            break;
        case 'f':
            fifo = optarg;
            break;
        }
    }


    if (!ifile.empty()){
        cout << "Loading data ..." << endl;
        loadData (ifile);
    }
    else {
        if (!fifo.empty()){
            cout << "Creating FIFO: " << fifo << endl;
            if (mkfifo(fifo.c_str(), 0777) == -1){
                cerr << "mkfifo error";
                return 0;
            }
            std::thread thrRecvData(loadDataFromFIFO, fifo);
            thrRecvData.detach();
        }
        else {
            std::thread thrRecvData(loadData, ifile);
            thrRecvData.detach();
        }
    }



    cout << "PID: " << getpid() << endl;
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = hdlSIGUSR1;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGUSR1);
    sigaction(SIGUSR1, &act, nullptr);


    cout << "Server start ..." << endl;
    int fd_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_sock < 0){
        perror("socket");
        exit(1);
    }

    sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd_sock, (sockaddr *)&addr, sizeof(addr)) < 0){
        perror("bind");
        exit(2);
    }



    const auto sz_buf {1024};
    char buf[sz_buf];
    while(f_run){
        memset(buf, 0, sz_buf);
        recvfrom (fd_sock, buf, sz_buf, 0, nullptr, nullptr);
        string data (buf);
        cout << "command: " << data << endl;

        if (data == "stop"){            // stop server
            f_run = false;
        }
        else{
            if (!ifile.empty())
                loadData (ifile);

            if (data == "STAT") {
                getFullStatistics();
            }
            else {
                getStatisticsEvent(data);
            }
        }
    }
    close(fd_sock);

    cout << "Server stop" << endl;
    return 0;
}
