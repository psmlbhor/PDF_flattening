#include <qpdf/QPDF.hh>

#include <iostream>

int main(int argc, char** argv)
{
    QPDF pdf;
    pdf.processFile(argv[1]);
    std::cout<<"Success"<<std::endl;
    return 0;    
}
