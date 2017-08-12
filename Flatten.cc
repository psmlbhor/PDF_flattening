#include <qpdf/QPDF.hh>
#include <qpdf/PointerHolder.hh>
#include <qpdf/Buffer.hh>
#include <qpdf/QPDFWriter.hh>

#include <iostream>
#include <cstdlib>
#include <sstream>
#include <cstring>

//Check if the given key exists in the dictionary
bool isKeyPresent(QPDFObjectHandle obj, std::string key)
{
    //ensure that obj is dictionary
    bool isDict = obj.isDictionary();
    
    if(isDict)
    {
        if(obj.hasKey(key))
            return true;
        else
            return false;
    }
    
    else
    {
        if(obj.getDict().hasKey(key))
            return true;
        else
            return false;
    }
}

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
            
            //count the XObjects
            int count = 0;

            //Create new content stream for the page
            std::string page_stream_contents = "q\n";

            //check if page has annotations
            if(!isKeyPresent(page,"/Annots"))
                continue;
            
            //Get all the annotations present in the page
            std::vector<QPDFObjectHandle> annotations = page.getKey("/Annots").getArrayAsVector();
           
            //check if the page's /Resources contains /XObject
            if(!isKeyPresent(page.getKey("/Resources"), "/XObject"))
            {
                QPDFObjectHandle empty_dictionary = QPDFObjectHandle::newDictionary();
                page.getKey("/Resources").getDict().replaceKey("/XObject",empty_dictionary);
            }

            //get the page resources
            std::map<std::string, QPDFObjectHandle> page_resources_xobject = page.getKey("/Resources").getKey("/XObject").getDictAsMap();
            
            //The content stream of the page should already be wrapped inside q...Q pair
            page.addPageContents(QPDFObjectHandle::newStream(&pdf, "q"), true);
            page.addPageContents(QPDFObjectHandle::newStream(&pdf, "Q"), false);

            //Work on every annotation present in the page
            std::vector<int> remove_annot;
            int annot_num=0;

            for(std::vector<QPDFObjectHandle>::iterator annot_iter = annotations.begin(); 
                annot_iter < annotations.end(); ++annot_iter)
            {
                QPDFObjectHandle annot = *annot_iter;
                if(!isKeyPresent(annot, "/F"))
                {
                    //Assuming it is not hidden and invisible and is allowed to print
                    QPDFObjectHandle number = QPDFObjectHandle::newInteger(4);
                    annot.getDict().replaceKey("/F", number);
                }

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
                    if(!isKeyPresent(annot,"/AP"))
                    {
                        QPDFObjectHandle dictionary = QPDFObjectHandle::newDictionary();
                        annot.replaceKey("/AP", dictionary);
                        QPDFObjectHandle normal_dictionary = QPDFObjectHandle::newDictionary();
                        annot.getKey("/AP").replaceKey("/N", normal_dictionary);
                        
                        //not sure what to do if annotation is present
                        //it effectively means that the annotation cannot be drawn
                    }

                    QPDFObjectHandle normal_appearance = annot.getKey("/AP").getKey("/N");
                    
                    //button might have /Yes or /Off states
                    if(annot.getKey("/FT").unparse()=="/Btn")
                    {
                        std::string appearance_state = annot.getKey("/AS").unparse();
                        std::cout<<annot.unparse()<<std::endl;
                        std::cout<<appearance_state<<std::endl;

                        //The state might not be present in /N dictionary in which
                        //case it should be fetched from /D dictionary
                        if(!isKeyPresent(normal_appearance, appearance_state))
                            normal_appearance = annot.getKey("/AP").getKey("/D").getKey(appearance_state);
                        else
                            normal_appearance = normal_appearance.getKey(appearance_state);
                    }
                    
                    //check if /XObject has /Name or not
                    if(!isKeyPresent(normal_appearance, "/Name"))
                    {
                        std::ostringstream xobj_count;
                        xobj_count << ++count;
                        std::string xobj_name = "/ResX";
                        xobj_name.append(xobj_count.str());

                        QPDFObjectHandle name = QPDFObjectHandle::parse(xobj_name);
                        normal_appearance.getDict().replaceKey("/Name", name);
                        page_stream_contents.append("\n1 0 0 1 0 0 cm "+xobj_name+" Do Q\nq\n");
                        
                        page_resources_xobject.insert(std::pair<std::string, QPDFObjectHandle>(xobj_name, normal_appearance));
                    }
                    else
                    {
                        QPDFObjectHandle xobj_name = normal_appearance.getDict().getKey("/Name");
                        page_stream_contents.append("\n1 0 0 1 0 0 cm "+xobj_name.unparse()+" Do Q\nq\n");

                        page_resources_xobject.insert(std::pair<std::string, QPDFObjectHandle>(xobj_name.unparse(), normal_appearance));
                    
                    }

                    //Apply transformations
                    //Insert XObject of /N
                    //Restore the graphics state
                    //
                    
                    //remove this annotation from the /Annots of the page
                    remove_annot.push_back(annot_num);  
                }
                annot_num++;
            }

            if(!isKeyPresent(page.getKey("/Resources"), "/XObject"))
            {
                QPDFObjectHandle xobj_dictionary = QPDFObjectHandle::newDictionary();
                page.getKey("/Resources").replaceKey("/XObject", xobj_dictionary);
            }

            //replace the orginal resources with the new one
            QPDFObjectHandle new_resources = pdf.makeIndirectObject(QPDFObjectHandle::newDictionary(page_resources_xobject));
            for(std::map<std::string, QPDFObjectHandle>::iterator it = page_resources_xobject.begin();
                it != page_resources_xobject.end(); ++it)
            {
                page.getKey("/Resources").getKey("/XObject").replaceKey(it->first, it->second);
            }

            page_stream_contents.append("Q\n");

            //check if the existing page contents are wrapped inside q...Q pair
            QPDFObjectHandle content = QPDFObjectHandle::newStream(&pdf, page_stream_contents);
            page.addPageContents(content, false);
            
            //replace the original /Annots array with the new array
            int pos_adjust = 0;
            for(size_t i=0; i<remove_annot.size(); ++i)
            {
                int num = remove_annot.front();
                annotations.erase(annotations.begin() + num + pos_adjust);
                pos_adjust++;
            }

            page.replaceKey("/Annots", QPDFObjectHandle::newArray(annotations));
        }
        
        //remove the AcroForm from the PDF
        pdf.getRoot().removeKey("/AcroForm");
        std::cout<<"PDF flattened successfully\nAcroForm removed"<<std::endl;
        //write the changes to the PDF
        QPDFWriter w(pdf, "output.pdf");
        w.write();

        //add preserved annotations to the page
    }    
    return 0;   
}
