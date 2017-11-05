#include <qpdf/QPDF.hh>
#include <qpdf/PointerHolder.hh>
#include <qpdf/Buffer.hh>
#include <qpdf/QPDFWriter.hh>

#include <iostream>
#include <cstdlib>
#include <sstream>
#include <cstring>
#include <exception>

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


//check if translation is needed for the annotation
bool needsTranslation(QPDFObjectHandle normal_appearance)
{
    bool translate = false;
    
    //if /Resources not present, then translation needed
    if(!isKeyPresent(normal_appearance, "/Resources"))
    {
        return true;
    }
    else
    {

        QPDFObjectHandle resources = normal_appearance.getDict().getKey("/Resources");
        
        //if /Resouces does not contain /XObject then needs translation
        if(!isKeyPresent(resources, "/XObject"))
        {
            return true;
        }
        else
        {
            std::map<std::string, QPDFObjectHandle> xobject;
            
            if(resources.isDictionary())
                xobject = resources.getKey("/XObject").getDictAsMap();
            else
                xobject = resources.getDict().getKey("/XObject").getDictAsMap();
           
            if(xobject.size()!=0)
            { 
                for(std::map<std::string, QPDFObjectHandle>::iterator it = xobject.begin();
                    it != xobject.end(); ++it)
                {
                    if(!isKeyPresent(it->second, "/BBox"))
                        continue;
                
                    double left = it->second.getDict().getKey("/BBox").getArrayItem(0).getNumericValue();
                    double bottom = it->second.getDict().getKey("/BBox").getArrayItem(1).getNumericValue();

                    //if /BBox does not start with (0,0) then it automatically takes
                    //care of the translation    
                    if(left==0 && bottom==0)
                    {
                        translate = true;
                        break;
                    }
                }
                return translate;
            } 
            
            return true;
        }
    }
    return true;
}

//Check if scaling is required
bool needsScaling(QPDFObjectHandle normal_appearance)
{
    
    if(!isKeyPresent(normal_appearance, "/Resources"))
    {
        return false;
    }
    else
    {
        QPDFObjectHandle resources = normal_appearance.getDict().getKey("/Resources");
        if(!isKeyPresent(resources,"/XObject"))
        {
            return false;
        }
        else
        {
            std::map<std::string, QPDFObjectHandle> xobjects;
            if(resources.isDictionary())
                xobjects = resources.getKey("/XObject").getDictAsMap();
            else
                xobjects = resources.getDict().getKey("/XObject").getDictAsMap();
            
            //If there are multiple xobjects present,
            //then scaling 'might' be required
            if(xobjects.size()==0)
                return false;
            else
                return true;
        }
    }
    return false;
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

void NoNeedAppearances(QPDF &pdf)
{
    std::cerr<<"DBG:\t Working with Acroform"<<std::endl;

        //Get all the pages present in the PDF document
        std::vector<QPDFObjectHandle> all_pages = pdf.getAllPages();
        for(std::vector<QPDFObjectHandle>::iterator page_iter = all_pages.begin();
            page_iter < all_pages.end(); ++page_iter)
        {
            std::cerr<<"DBG:\t Working on a new page"<<std::endl;

            QPDFObjectHandle page = *page_iter;
            std::vector<QPDFObjectHandle> preserved_annots;
            
            //count the XObjects
            int count = 0;

            //Create new content stream for the page
            std::string page_stream_contents = "q\n";

            //check if page has annotations
            if(!isKeyPresent(page,"/Annots"))
                continue;
            
            std::cerr<<"DBG:\t Accessing page Annots"<<std::endl;
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
                std::cerr<<"DBG:\t Working on a new Annot"<<std::endl;

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
                    std::cerr<<"DBG:\t Flags honoured"<<std::endl;
                    if(!isKeyPresent(annot,"/AP"))
                    {
                        QPDFObjectHandle normal_dictionary = QPDFObjectHandle::newStream(&pdf);
                        std::map<std::string, QPDFObjectHandle> N;
                        N.insert(std::pair<std::string, QPDFObjectHandle>("/N", normal_dictionary));
                        annot.replaceKey("/AP", QPDFObjectHandle::newDictionary(N));
                    }

                    std::cerr<<"DBG:\t Accessing Annot's /AP and /N"<<std::endl;
                    QPDFObjectHandle normal_appearance = annot.getKey("/AP").getKey("/N");

                    
                    //button might have /Yes or /Off states
                    if(annot.getKey("/FT").unparse()=="/Btn")
                    {
                        std::string appearance_state = annot.getKey("/AS").unparse();

                        std::cerr<<"DBG:\t Changing normal_appearance for /Btn"<<std::endl;
                        //The state might not be present in /N dictionary in which
                        //case it should be fetched from /D dictionary
                        if(!isKeyPresent(normal_appearance, appearance_state))
                            normal_appearance = annot.getKey("/AP").getKey("/D").getKey(appearance_state);
                        else
                            normal_appearance = normal_appearance.getKey(appearance_state);
                    }


                    bool isNAdict = false;
                    if(normal_appearance.isDictionary())
                        isNAdict = true;
                    
                    //check if /XObject has /Name or not
                    std::string replace_name = "";
                    if(!isKeyPresent(normal_appearance, "/Name"))
                    {
                        std::cerr<<"DBG:\t Standard /XObject name not present"<<std::endl;
                        std::ostringstream xobj_count;
                        xobj_count << ++count;
                        std::string xobj_name = "/ResX";
                        xobj_name.append(xobj_count.str());

                        QPDFObjectHandle name = QPDFObjectHandle::newName(xobj_name);
                        if(!isNAdict) //it is a stream
                            normal_appearance.getDict().replaceKey("/Name", name);
                        else //it is a dictionary
                            normal_appearance.replaceKey("/Name", name);

                        replace_name = xobj_name;
                    }
                    else
                    {
                        std::cerr<<"DBG:\t Standard /XObject name present"<<std::endl;
                        QPDFObjectHandle xobj_name;
                        if(!isNAdict) //it is a stream
                            xobj_name = normal_appearance.getDict().getKey("/Name");
                        else //it is a dictionary
                            xobj_name = normal_appearance.getKey("/Name");
                        replace_name = xobj_name.unparse(); 
                    }
                    
                    //Insert the named XObject into /Resources
                    page_resources_xobject.insert(std::pair<std::string, QPDFObjectHandle>(replace_name, normal_appearance));
                    
                    //APPLY TRANSFORMATIONS
                    //Check if translation is required
                    bool translate = needsTranslation(normal_appearance);
                    double transformation_matrix[6] = {1,0,0,1,0,0};

                    //Get the llx and lly values of the annotation rectangle
                    if(translate)
                    {
                        std::cerr<<"DBG:\t Performing translation"<<std::endl;
                        transformation_matrix[4] = annot.getKey("/Rect").getArrayItem(0).getNumericValue();
                        transformation_matrix[5] = annot.getKey("/Rect").getArrayItem(1).getNumericValue();
                    }

                    //Check if scaling is required
                    bool scaling = needsScaling(normal_appearance);
                    
                    if(scaling)
                    {
                        std::cerr<<"DBG:\t Performaing scaling"<<std::endl;
                        double BBox[4] = {0,0,0,0};
                        double rect[4] = {0,0,0,0};

                        for(int i=0; i<4; ++i)
                        {
                            if(isNAdict)
                                BBox[i] = normal_appearance.getKey("/BBox").getArrayItem(i).getNumericValue();
                            else
                                BBox[i] = normal_appearance.getDict().getKey("/BBox").getArrayItem(i).getNumericValue();
                            rect[i] = annot.getKey("/Rect").getArrayItem(i).getNumericValue();
                        }

                        double BBox_width = BBox[2] - BBox[0];
                        double BBox_height = BBox[3] - BBox[1];

                        double rect_width = rect[2] - rect[0];
                        double rect_height = rect[3] - rect[1];

                        //If width of BBox and Rectangle do not match then the
                        //transformation matrix should scale the BBox to the
                        //size of the annotation Rectangle
                        if(BBox_width - rect_width != 0 && BBox_height - rect_height !=0)
                        {
                            double scaleX = rect_width / BBox_width;
                            double scaleY = rect_height / BBox_height;

                            transformation_matrix[0] = scaleX;
                            transformation_matrix[3] = scaleY;
                        }

                    }

                    std::ostringstream conv[4];
                    conv[0] << transformation_matrix[0];
                    conv[1] << transformation_matrix[3];
                    conv[2] << transformation_matrix[4];
                    conv[3] << transformation_matrix[5];

                    page_stream_contents.append("\n" + conv[0].str() + " 0 0 " + conv[1].str() +" " +
                                                conv[2].str() + " " + conv[3].str() + " cm " +
                                                replace_name + " Do Q\nq\n");
                    
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
        std::cerr<<"PDF flattened successfully\nAcroForm removed"<<std::endl;

        //write the changes to the PDF
        //QPDFWriter w(pdf, "output.pdf");
        //w.write();

}
static int count = 0;
void generateOneAppearance(QPDFObjectHandle &field, QPDFObjectHandle &annotation, std::map<std::string, std::string> inherited, QPDF &pdf)
{
    if(!annotation.hasKey("/AP"))
    {
        //Get the /Rect entry from the annotation
        double rect[4] = {0, 0, 0, 0};
        for(int i = 0; i < 4; i++)
        {
            rect[i] = annotation.getKey("/Rect").getArrayItem(i).getNumericValue();
        }

        //Get the Default Resources from the AcroForm
        QPDFObjectHandle defaultResources = QPDFObjectHandle::newDictionary();
        if(pdf.getRoot().getKey("/AcroForm").hasKey("/DR"))
        {
            defaultResources = pdf.getRoot().getKey("/AcroForm").getKey("/DR");
        }
        
        
        //If there exists a Normal Apperance stream already, then we have to replace
        //the existing /Resources with the new one, but care has to be taken that if
        //the existing resources contains conflicting names, then the existing names
        //are to be given preference
        //QPDFObjectHandle defaultResources = QPDFObjectHandle::newDictionary();
        //for(std::map<std::string, QPDFObjectHandle>::it = DRmap.begin();
        //        it != DRmap.end(); it++)
        //{
            
        //}
        //

        QPDFObjectHandle BBoxArray = QPDFObjectHandle::newArray();
        BBoxArray.appendItem( QPDFObjectHandle::newReal(0) );
        BBoxArray.appendItem( QPDFObjectHandle::newReal(0) );
        BBoxArray.appendItem( QPDFObjectHandle::newReal(rect[2]) );
        BBoxArray.appendItem( QPDFObjectHandle::newReal(rect[3]) );

        std::ostringstream s[2];
        s[0] << defaultResources.getObjectID();
        s[1] << defaultResources.getGeneration();
        QPDFObjectHandle normalDictionary = QPDFObjectHandle::newDictionary();
        QPDFObjectHandle type = QPDFObjectHandle::newName("/XObject");
        QPDFObjectHandle subtype = QPDFObjectHandle::newName("/Form");
        normalDictionary.replaceKey("/Type", type);
        normalDictionary.replaceKey("/Subtype", subtype);
        normalDictionary.replaceKey("/Resources", defaultResources);
        normalDictionary.replaceKey("/BBox", BBoxArray);

        std::string streamContent = "/Tx BMC q BT " + inherited.find("/DA")->second + " 1 0 0 1 0 0 Tm\n"
                                    "(" + inherited.find("/V")->second + ") Tj ET Q EMC\n";


        QPDFObjectHandle normalAppearance = QPDFObjectHandle::newStream(&pdf, streamContent);
        normalAppearance.replaceDict(normalDictionary);
    
        //Add the /AP << /N object >> entry to the annotation
        std::map<std::string, QPDFObjectHandle> N;
        N.insert(std::pair<std::string, QPDFObjectHandle>("/N", normalAppearance));
        annotation.replaceKey("/AP", QPDFObjectHandle::newDictionary(N));

        //QPDFWriter w(pdf, "trueoutput.pdf");
        //w.write();
    }
    else
    {
        QPDFObjectHandle currentNormalAppearance = annotation.getKey("/AP").getKey("/N");
        QPDFObjectHandle appearanceObject;
        
        if(currentNormalAppearance.isDictionary())
        {
            appearanceObject = currentNormalAppearance.getKey(annotation.getKey("/AS").unparse());
        }
        else
        {
            appearanceObject = currentNormalAppearance;
        }

        
        //Get the Default Resources from the AcroForm
        QPDFObjectHandle defaultResources = QPDFObjectHandle::newDictionary();
        if(pdf.getRoot().getKey("/AcroForm").hasKey("/DR"))
        {
            defaultResources = pdf.getRoot().getKey("/AcroForm").getKey("/DR");
        }
        appearanceObject.getDict().replaceKey("/Resources", defaultResources);
        std::string streamContent = "\n/Tx BMC" 
                                    "\nq" 
                                    "\nBT" 
                                    "\n" + inherited.find("/DA")->second + 
                                    "\n" + "1 0 0 1 0 0 Tm" 
                                    "\n(" + inherited.find("/V")->second + ") Tj" 
                                    "\nET" 
                                    "\nQ" 
                                    "\nEMC\n";
        
        appearanceObject.replaceStreamData(streamContent, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());

        //QPDFWriter w(pdf, "trueoutput.pdf");
        //w.write();
    }
}   

void getInheritableValues(QPDFObjectHandle &parent, QPDFObjectHandle &object, std::map<std::string, std::string> inherited_values, QPDF &pdf)
{
    //check for the inheritable attributes
    std::map<std::string, std::string> inherited;
    inherited = inherited_values;

    if(object.hasKey("/FT"))
    {
        inherited.insert(std::pair<std::string, std::string>("/FT", object.getKey("/FT").unparse()));
    }
    if(object.hasKey("/Ff"))
    {
        inherited.insert(std::pair<std::string, std::string>("/Ff", object.getKey("/Ff").unparse()));
    }
    if(object.hasKey("/V"))
    {
        inherited.insert(std::pair<std::string, std::string>("/V", object.getKey("/V").unparse()));
    }
    if(object.hasKey("/DV"))
    {
        inherited.insert(std::pair<std::string, std::string>("/DV", object.getKey("/DV").unparse()));
    }
    if(object.hasKey("/DA"))
    {
        inherited.insert(std::pair<std::string, std::string>("/DA", object.getKey("/DA").unparse()));
    }
    if(object.hasKey("/Q"))
    {
        inherited.insert(std::pair<std::string, std::string>("/Q", object.getKey("/Q").unparse()));
    }

    //Check if the field is field dictionary
    if(object.hasKey("/Kids"))
    {
        //recursively descend into children as well
        std::vector<QPDFObjectHandle> children = object.getKey("/Kids").getArrayAsVector();
        for(std::vector<QPDFObjectHandle>::iterator child_iterator = children.begin();
            child_iterator < children.end(); ++child_iterator)
        {
            QPDFObjectHandle child = *child_iterator;
            getInheritableValues(object, child, inherited, pdf);
        }
    }
    //If not kids but only parent is present, then the object has to be
    //both, a field dictionary as well as annotation dictionary
    else if(object.hasKey("/Parent"))
    {
        if(object.getKey("/FT").unparse() == "/Tx")
            generateOneAppearance(object, object, inherited, pdf);
    }
    //If both are not present, then that means it is a top level annotation with
    //Field and annotation dictionaries merged together
    else
    {
        if(object.getKey("/FT").unparse() == "/Tx")
            generateOneAppearance(parent, object, inherited, pdf);
    }
}

void needAppearances(QPDF &pdf)
{

    //Get all the pages present in the PDF document
    std::vector<QPDFObjectHandle> all_pages = pdf.getAllPages();
    for(std::vector<QPDFObjectHandle>::iterator page_iter = all_pages.begin();
        page_iter < all_pages.end(); ++page_iter)
    {
        std::cerr<<"DBG:\t Working on a new page"<<std::endl;
        QPDFObjectHandle page = *page_iter;

        if(!isKeyPresent(page,"/Annots"))
            continue;

        //Get all the annotations present in the page
        std::vector<QPDFObjectHandle> annotations = page.getKey("/Annots").getArrayAsVector();

        //Iterate over every annotation present on the page       
        for(std::vector<QPDFObjectHandle>::iterator annot_iter = annotations.begin(); 
            annot_iter < annotations.end(); ++annot_iter)
        {
            std::cerr<<"DBG:\t Working on a new Annot"<<std::endl;
            QPDFObjectHandle annot = *annot_iter;
            std::map<std::string, std::string> inherited_values;
            getInheritableValues(annot, annot, inherited_values, pdf);
            count++;
        }
    }
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
        QPDFObjectHandle root = pdf.getRoot();
        if(!root.getKey("/AcroForm").hasKey("/NeedAppearances") ||
           root.getKey("/AcroForm").getKey("/NeedAppearances").unparse() != "true")
        {
            NoNeedAppearances(pdf);
        }
        else
        {
            std::cerr<<"This PDF form needs generation of appearances"<<std::endl;
            needAppearances(pdf);
            root.getKey("/AcroForm").removeKey("/NeedAppearances");
            NoNeedAppearances(pdf);    
        }
        QPDFWriter w(pdf, "output.pdf");
        w.write();
    }    
    return 0;   
}
