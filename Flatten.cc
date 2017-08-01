#include <qpdf/QPDF.hh>

#include <iostream>
#include <cstdlib>

bool acroformPresent(QPDF &pdf)
{
    QPDFObjectHandle root = pdf.getRoot();
    if(!root.hasKey("/AcroForm"))
    {
        std::cerr << "Acroform not present in the PDF" << std::endl;
        return false;
    }

    std::cerr << "Acroform present in the PDF" << std::endl;
    return true;
}

void usage()
{
    std::cerr << "Usage: ./Flatten <input_file>" << std::endl;
    exit(2);
}

int main(int argc, char** argv)
{
    QPDF pdf;
    if(argc<2)
    {
        usage();
    }

    pdf.processFile(argv[1]);
    
    //Check if /AcroForm is present, and if it is present
    //then process further
    if(acroformPresent(pdf))
    {
        //Get the object ID of those present in /Fields[] 
        QPDFObjectHandle root = pdf.getRoot();
        QPDFObjectHandle acroform_dict = root.getKey("/AcroForm");
        QPDFObjectHandle acroform_fields = acroform_dict.getKey("/Fields");
        int objectID[acroform_fields.getArrayNItems()];
        for(int i=0; i<acroform_fields.getArrayNItems(); ++i)
        {
            QPDFObjectHandle temp = acroform_fields.getArrayItem(i);
            objectID[i] = temp.getObjectID();
            std::cerr << objectID[i] << std::endl;
        }

        //std::cerr << "arcroform_dict: " << acroform_dict.getName();
            
    }    
    return 0;   
}
