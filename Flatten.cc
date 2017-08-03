#include <qpdf/QPDF.hh>
#include <qpdf/PointerHolder.hh>
#include <qpdf/Buffer.hh>

#include <iostream>
#include <cstdlib>
#include <sstream>

//Function to check if the annotation is allowed to be printed
bool annotationAllowed(unsigned int flags)
{
    bool value = true;
    int bit = 0x01;   
    
    //check if annotation is invisible
    if(flags & bit)
        value = false;

    //check if annotation is hidden
    else if(flags & (bit<<1))
        value = false;

    //check if allowed to print
    else if(!(flags & (bit<<2)))
        value = false;

    return value;
}   

//Check whether the document consists of AcroForm
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
        /*
        QPDFObjectHandle root = pdf.getRoot();
        QPDFObjectHandle acroform_dict = root.getKey("/AcroForm");
        QPDFObjectHandle acroform_fields = acroform_dict.getKey("/Fields");
        int objectID[acroform_fields.getArrayNItems()];
        for(int i=0; i<acroform_fields.getArrayNItems(); ++i)
        {
            QPDFObjectHandle temp = acroform_fields.getArrayItem(i);
            objectID[i] = temp.getObjectID();
            //std::cerr << objectID[i] << std::endl;
        }
        
        //Get the default resources for the AcroForm
        QPDFObjectHandle default_resources_dict = acroform_dict.getKey("/DR"); 
        QPDFObjectHandle default_resources_obj = pdf.getObjectByID(default_resources_dict.getObjectID(), default_resources_dict.getGeneration());
        

        QPDFObjectHandle str_obj = pdf.getObjectByID(50,0);
        PointerHolder<Buffer> b = str_obj.getStreamData();
        std::cout << b.getPointer()->getBuffer();
        std::vector<QPDFObjectHandle> all_pages = pdf.getAllPages();
        
        //Process flattening on every page
        std::vector<QPDFObjectHandle>::iterator it;
        for(it = all_pages.begin() ; it < all_pages.end() ; ++it)
        {
            
        }
        */
        
        //Get all the pages present in the PDF document
        std::vector<QPDFObjectHandle> all_pages = pdf.getAllPages();
        for(std::vector<QPDFObjectHandle>::iterator page_iter = all_pages.begin();
            page_iter < all_pages.end(); ++page_iter)
        {
            QPDFObjectHandle page = *page_iter;
            std::vector<QPDFObjectHandle> preserved_annots;

            //Get all the annotations present in the page
            std::vector<QPDFObjectHandle> annotations = page.getKey("/Annots").getArrayAsVector();
            
            //Work on every annotation present in the page
            for(std::vector<QPDFObjectHandle>::iterator annot_iter = annotations.begin(); 
                annot_iter < annotations.end(); ++annot_iter)
            {
                QPDFObjectHandle annot = *annot_iter;
                std::stringstream s(annot.getKey("/F").unparse());
                unsigned int flags = 0;
                s >> flags;
                //preserve non-widget type annotations
                if(annot.getKey("/Subtype").unparse() != "/Widget")
                {
                    preserved_annots.push_back(annot);    
                }
                //Honour the flags(/F) present in the annotation
                else if (annotationAllowed(flags))
                {
                    //
                    std::cout << page.getKey("/Contents").getStreamData().getPointer()->getBuffer() << std::endl;
                    //get normal appearance of widget
                    //save graphics state
                    //Apply transformations
                    //Insert XObject of /N
                    //Restore the graphics state 
                }
            } 
        }
        //add preserved annotations to the page
    }    
    return 0;   
}
