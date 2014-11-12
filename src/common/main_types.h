#include <string>
#include <fstream>

class DisplayThreadTasks
{
public:
        virtual void add_task(std::function<bool()>&& task) = 0;
};

class FileSystem
{
public:
        virtual std::ifstream open_file(std::string relpath) const = 0;
};
