#include <qpdf/QPDF.hh>
#include <qpdf/PointerHolder.hh>
#include <qpdf/Buffer.hh>
#include <qpdf/QPDFWriter.hh>

#include <iostream>
#include <cstdlib>
#include <sstream>
#include <cstring>

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

        //Get all the pages present in the PDF document
        std::vector<QPDFObjectHandle> all_pages = pdf.getAllPages();
        for(std::vector<QPDFObjectHandle>::iterator page_iter = all_pages.begin();
            page_iter < all_pages.end(); ++page_iter)
        {
            QPDFObjectHandle page = *page_iter;
            std::vector<QPDFObjectHandle> preserved_annots;
            
            /*
            unsigned char* page_stream = page.getKey("/Contents").getStreamData().getPointer()->getBuffer();
            std::string content_stream;
            if(page_stream)
            {
                content_stream(reinterpret_cast<char*>(page_stream));
            }
            */
            
            //count the XObjects
            int count = 0;

            //Create new content stream for the page
            QPDFObjectHandle page_stream;
            std::string page_stream_contents = "q\n";

            //Get all the annotations present in the page
            std::vector<QPDFObjectHandle> annotations = page.getKey("/Annots").getArrayAsVector();
            
            //get the page resources;
            std::map<std::string, QPDFObjectHandle> page_resources_xobject = page.getKey("/Resources").getKey("/XObject").getDictAsMap();

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
                else if (annotationAllowed(flags)&&(std::cout<<"Maybe this one"<<std::endl))
                {
                    //
                    //std::cout << page.getKey("/Contents").getStreamData().getPointer()->getBuffer() << std::endl;

                    //Get the normal appearnce of the widget
                    //QPDFObjectHandle normal_appearence = annot.getKey("/AP").getKey("/N"); 
                    
                    //Get the rectangle position of the widget
                    /*
                    QPDFObjectHandle rectangle_position = annot.getKey("/Rect");
                    std::vector<double> widget_rectangle;
                    for(int i=0; i<4; ++i)
                    {
                        widget_rectangle.push_back(rectangle_position.getArrayItem(i).getNumericValue());
                        std::ostringstream temp;
                        temp << widget_rectangle[i];
                        page_stream_contents.append(temp.str()+" ");
                    }
                    page_stream_contents.append("re B*\n");
                    std::cout<<page_stream_contents<<std::endl;
                    */
                    
                    QPDFObjectHandle normal_appearence = annot.getKey("/AP").getKey("/N").getDict();
                    std::cout<<"The problem is above"<<std::endl;
                    // add name to the XObject of /N
                    if((std::cout<<"FIRST"<<std::endl) && !normal_appearence.hasKey("/Name"))
                    {
                        std::ostringstream xobj_count;
                        xobj_count << ++count;
                        std::string xobj_name = "/ResX";
                        xobj_name.append(xobj_count.str());

                        QPDFObjectHandle name = QPDFObjectHandle::parse(xobj_name);
                        std::cout<<"HERE"<<std::endl;
                        normal_appearence.replaceKey("/Name", name);
                        std::cout<<"THERE"<<std::endl;
                        page_stream_contents.append("\n1 0 0 1 0 0 cm "+xobj_name+" Do Q\nq");
                        
                        page_resources_xobject.insert(std::pair<std::string, QPDFObjectHandle>(xobj_name, normal_appearence));
                        std::cout<<"I think this one"<<std::endl;
                    }
                    else
                    {
                        std::cout<<"NEWHERE"<<std::endl;
                        std::cout.flush();
                    //add the XObject to the page's resources
                        QPDFObjectHandle xobj_name = normal_appearence.getKey("/Name");
                        page_stream_contents.append("\n1 0 0 1 0 0 cm "+xobj_name.unparse()+" Do Q\nq");

                        page_resources_xobject.insert(std::pair<std::string, QPDFObjectHandle>(xobj_name.unparse(), normal_appearence));
                    
                    }

                    //Apply transformations
                    //Insert XObject of /N
                    //Restore the graphics state 
                }
            } 
            std::cout<<"Performaing final rights"<<std::endl;
            page_stream_contents.append("Q\n");
            QPDFObjectHandle content = QPDFObjectHandle::newStream(&pdf, page_stream_contents);
            page.addPageContents(content, false);
            std::cout<<"Final rights performed"<<std::cout;
            //restore graphics state
        }
        
        //remove the AcroForm from the PDF
        pdf.getRoot().removeKey("/AcroForm");
        std::cout<<"AcroForm removed"<<std::endl;

        //write the changes to the PDF
        QPDFWriter w(pdf, "output.pdf");
        w.write();

        //add preserved annotations to the page
    }    
    return 0;   
}
