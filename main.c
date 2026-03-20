#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define SMODEL_WRITE 2
#define SMODEL_WRITEFREE 1
#define SMODEL_FREE 0

#define SMODEL_VERSION 2
#define SMODEL_MAGIC_NUMBER 6767

// compare strings function
int cmat(char* a, char* b) {
    char *ca=a, *cb=b;
    while (*(ca++)==*(cb++)) if (*ca==0&&*cb==0) return 1;
    return 0;
}
int lcmat(char* a, char* b, int maxlen) {
    char *ca=a, *cb=b;
    while (*(ca++)==*(cb++)) if ((ca-a)>maxlen-1) return 1;
    return 0;
}

// power function
int apow(int a, int b) {
    int result;
    for (int i=0, result=a;i<b-1;i++,result*=a);
    return (b==0)?1:result;
}

// my code-golf solution to string to int (signed, multuple digit, not recursive, 111 bytes long)
int stoi(char* s){char* c=s-1;int r=0,n=(*s==45)?*c++*0+1:0;while (*(++c)!=0) r=r*10+(*c-48);return r*(1-n*2);}

struct lcollection {
    uint32_t letter_count;
    uint32_t before;
    char* after;
    int set;

    struct lcollection* next;
};

struct smodel {
    uint8_t version;
    uint16_t magic_number;

    uint16_t name_length;
    char* name;

    uint8_t before_length;

    uint32_t file_length;
    char* sfile;

    uint32_t lc_count;
    struct lcollection lc_first;
};

int print_help() {
    printf("bot help\n\
\n\
-i [file]   |  specify input file\n\
-o [file]   |  creates or writes to a file for output\n\
-l [i]      |  set target length for output, or change target training memory size\n\
-c [file]   |  combine this smodel with the input file and write to output file\n\
-t, --train |  changes the mode to training mode, where it will create a model\n\
-h, --help  |  show this menu\n");
}

char* input_file = NULL;
char* combine_file_child = NULL;
char* output_file = NULL;

int print_len = -1;
int write_file = 0;

int do_train = 0;

int lc_size = 2;

// Write model to file and free memory
void write_smodel(struct smodel* model, int mode) {
    if (mode<0||mode>2) {
        printf("write_smodel needs a valid mode\n");
        return;
    }
    if (output_file==NULL&&mode>0){
        printf("Tried to write with no output file?\n");
        return;
    }
    
    FILE* fp;

    if (mode>0) {
        fp = fopen(output_file,"wb");
        if (fp==NULL) {
            printf("Failed to open file for writing?\n");
            return;
        }
    }

    if (mode>0) {
        fwrite(&model->version,1,1,fp); // version
        
        fwrite(&model->magic_number,2,1,fp);
        
        fwrite(&model->name_length,2,1,fp); // name length
        fwrite(model->name,1,model->name_length,fp); // name

        fwrite(&model->file_length,2,1,fp); // file length
        fwrite(model->sfile,1,model->file_length,fp); // file contents
        
        fwrite(&model->before_length,1,1,fp); // before length
        fwrite(&model->lc_count,4,1,fp); // lc count
    }

    if (mode!=2)
        free(model->sfile);
        free(model->name);

    struct lcollection* curr = &model->lc_first; // selected lcollection

    struct lcollection* tofree; // lcollection to free
    int onfirst=1;
    while (1) {
        if (mode>0) {
            fwrite(&curr->letter_count,4,1,fp);

            fwrite(&curr->before,4,1,fp);
        
            fwrite(curr->after,1,curr->letter_count,fp);
        }

        if (mode!=2) {
            free(curr->after);
        }
        if (curr->next==NULL&&mode!=2) {
            if (!onfirst)
                free(curr);
            break;
        }
        tofree=curr;
        curr=curr->next;
        if (mode!=2&&!onfirst)
                free(tofree);
        onfirst=0;
    }

    if (mode>0)
        fclose(fp);
    if (mode!=2)
        free(model);
}

// train file into model
struct smodel* train() {
    if (input_file==NULL){
        printf("Tried to train with no input file?\n");
        return NULL;
    }
    
    FILE* fp;

    fp = fopen(input_file,"rb");
    if (fp==NULL) {
        printf("Failed to open file \"%s\" for reading",input_file);
        return NULL;
    }

    struct smodel* model = malloc(sizeof(struct smodel));
    model->version=SMODEL_VERSION;
    model->name=malloc(sizeof(char)*(strlen(input_file)+1));
    model->name_length=strlen(input_file)+1;
    snprintf(model->name,(strlen(input_file)+1),"%s",input_file);

    fseek(fp, 0L, SEEK_END);
    long file_size = ftell(fp);
    
    model->file_length=0;
    model->sfile=file_size;

    model->before_length=lc_size;
    model->lc_count=0;

    char* lc = malloc((size_t)lc_size);
    char curr;

    uint32_t index=0;

    uint32_t size;

    printf("Training model: \033[s");

    struct lcollection* lc_last=&model->lc_first;

    if (fseek(fp, 0, SEEK_END) == 0) {
        size = ftell(fp);
    }
    fseek(fp, 0, SEEK_SET);

    while (index<size) {
        if (index%(size/100)==0) {
            printf("\033[u%i%%",(int)(100*((float)index/size))+1);
            fflush(stdout);
        }
        if (index>lc_size) {
            int found_index=-1;
            struct lcollection* cur=&model->lc_first;

            for (int i=0;i<model->lc_count;i++) {
                int matched=0;
                for (int x=0;x<model->before_length;x++) {
                    if (lc[x]==cur->before[x]) {
                        matched++;
                    }
                }

                if (matched==model->before_length) {
                    found_index=i;
                    break;
                }
                
                cur=cur->next;
            }

            if (found_index==-1) {
                if (model->lc_count!=0){
                    lc_last->next=malloc(sizeof(struct lcollection));
                    lc_last=lc_last->next;
                }

                lc_last->set=0;
                lc_last->letter_count=1;

                lc_last->before=malloc((size_t)lc_size);
                for (int i=0;i<lc_size;i++) {
                    lc_last->before[i]=lc[i];
                }
                
                lc_last->after=malloc(sizeof(char));
                *lc_last->after = curr;

                lc_last->next=NULL;
                model->lc_count++;
            } else {
                cur->after=realloc(cur->after,sizeof(char)*(cur->letter_count+1));
                
                cur->after[cur->letter_count] = curr;
                cur->letter_count++;
            }
        }
        for (int i=0;i<lc_size;i++) {
            if (i+1==lc_size) {
                lc[i]=curr;
                break;
            }
            lc[i]=lc[i+1];
        }
        fread(&curr, 1, 1, fp);
        index++;
    }

    printf("\n");

    free(lc);
    fclose(fp);

    return model;
}

void generate(struct smodel* model, int target, int dowrite) {
    
    char* lc = malloc((size_t)model->before_length);

    FILE* fp=NULL;
    if (dowrite) {
        fp=fopen(output_file,"wb");
        if (fp==NULL) {
            printf("Failed to open file for writing?\n");
            return;
        }
    }

    #define BUFFER_SIZE 500

    char buffer[BUFFER_SIZE] = {};
    int buffer_count=0;

    void write_char(char ch) {
        if (buffer_count==BUFFER_SIZE-1) {
            fwrite(&buffer[0],1,buffer_count+1,(fp==NULL)?stdout:fp);
            buffer_count=0;
        } else {
            buffer[buffer_count]=ch;
            buffer_count++;
        }
    }

    if (dowrite)
        printf("Generating file: \033[s");

    // populate lc with first values
    for (int i=0;i<model->before_length;i++) {
        lc[i]=model->lc_first.before[i];
        write_char(lc[i]);
    }

    for (int i=model->before_length;i<target;i++) {
        // Find matching LC
        struct lcollection* curr = &model->lc_first;
        int wasfound=0;
        for (int x=0;x<model->lc_count;x++) {
            if (lcmat(lc,curr->before,model->before_length)) {
                wasfound=1;
                break;
            }
            if (curr->next==NULL) {
                break;
            }
            curr=curr->next;
        }

        if (!wasfound) { // No letter found to come next, presume end of file, but to match exact file length i reset the context
            for (int i=0;i<model->before_length;i++) {
                lc[i]=model->lc_first.before[i];
                write_char(lc[i]);
            }
            continue;
        }

        if (i%(target/100)==0&&dowrite) {
            printf("\033[u%i%%",(int)(100*((float)i/target))+1);
            fflush(stdout);
        }

        // Randomly pick letter
        char chosen = curr->after[rand()%curr->letter_count];
        write_char(chosen);

        for (int x=0;x<model->before_length;x++) {
            if (x==model->before_length-1) {
                lc[x]=chosen;
            break;
            }
            lc[x]=lc[x+1];
        }
    }

    fwrite(&buffer[0],1,buffer_count+1,(fp==NULL)?stdout:fp);
    if (dowrite)
        printf("\n");
    if (fp!=NULL) {
        fclose(fp);
    }
    free(lc);
}

struct smodel* read_file(char* filename) {
    FILE* fp;

    fp = fopen(filename,"rb");
    if (fp==NULL) {
        printf("cant open file %s?",filename);
        return NULL;
    }

    struct smodel* model = malloc(sizeof(struct smodel));

    uint8_t version;
    fread(&version, 1, 1, fp);

    if (version!=SMODEL_VERSION) {
        printf("model version %i not supported",version);
        return NULL;
    }

    uint16_t namelen;
    fread(&namelen, 2, 1, fp);

    model->version=SMODEL_VERSION;
    model->name_length=namelen;
    model->name=malloc(namelen);

    fread(model->name, 1, namelen, fp);

    fread(&model->before_length, 1, 1, fp);
    fread(&model->lc_count, 4, 1, fp);
    
    struct lcollection* curr = &model->lc_first;
    for (int i=0;i<model->lc_count;i++) {
        fread(&curr->letter_count, 4, 1, fp);
        curr->before = malloc(model->before_length);
        fread(curr->before, 1, model->before_length, fp);
        
        curr->after = malloc(curr->letter_count);
        fread(curr->after, 1, curr->letter_count, fp);

        if (i!=model->lc_count-1) {
            curr->next = malloc(sizeof(struct lcollection));
            curr->next->next=NULL;
            curr=curr->next;
        }
    }
    curr->next=NULL;

    fclose(fp);
    return model;
}

void print_model(struct smodel* model) {
    struct lcollection* curr=&model->lc_first;
    for (int i=0;i<model->lc_count;i++) {

        printf("collection: \"");
        for (int x=0;x<model->before_length;x++) {
            printf("%c",curr->before[x]);
        }
        printf("\", after: ");
        for (int x=0;x<curr->letter_count;x++) {
            printf("%c, ",curr->after[x]);
        }
        printf("\n\n");
                
        if (curr->next==NULL) {
            break;
        }
        curr=curr->next;
    }
}

struct smodel* combine_models(struct smodel* parent1, struct smodel* child1, char* name) {

    struct smodel* model = malloc(sizeof(struct smodel));

    model->version=SMODEL_VERSION;

    if (name!=NULL) {
        model->name_length=strlen(name)+1;
        model->name=malloc(model->name_length);
        memcpy(model->name,name,model->name_length);
    } else {
        // Combine names with "+"
        model->name_length=parent1->name_length+child1->name_length;
        model->name=malloc(parent1->name_length+child1->name_length);
        sprintf(model->name,"%s",parent1->name);
        model->name[parent1->name_length-1]='+';
        sprintf((model->name+parent1->name_length),"%s",child1->name);
        model->name[parent1->name_length+child1->name_length-1]='\0';
    }

    // The model with a higher before length should always be the child
    struct smodel* parent = parent1;
    struct smodel* child = child1;

    model->before_length=parent->before_length;

    if (parent->before_length>child->before_length) {
        parent=child1;
        child=parent1;
    }

    model->lc_count=0;


    struct lcollection* last;

    // Add parents lc to new model
    struct lcollection* curr=&model->lc_first;
    struct lcollection* pcurr=&parent->lc_first;
    for (int i=0;i<parent->lc_count;i++) {
        curr->letter_count=pcurr->letter_count;

        curr->before=malloc(parent->before_length);
        curr->after=malloc(curr->letter_count);

        memcpy(curr->before,pcurr->before,parent->before_length);
        memcpy(curr->after,pcurr->after,curr->letter_count);

        if (pcurr->next!=NULL){
            curr->next=malloc(sizeof(struct lcollection));
            curr=curr->next;
        }
        model->lc_count++;
        curr->next=NULL;
        pcurr=pcurr->next;
    }
    last=curr;

    int bef_diff = parent->before_length - child->before_length + 1;
    
    curr=&child->lc_first;
    for (int i=0;i<child->lc_count;i++) {

        for (int x=0;x<bef_diff;x++) {
            int found_index=-1;
            struct lcollection* cur=&model->lc_first;

            for (int w=0;w<model->lc_count;w++) {
                int matched=0;
                for (int z=0;z<model->before_length;z++) {
                    if (curr->before[z+x]==cur->before[z]) {
                        matched++;
                    }
                }

                if (matched==model->before_length) {
                    found_index=w;
                    break;
                }
                
                cur=cur->next;
            }

            if (found_index==-1) {
                last->next=malloc(sizeof(struct lcollection));
                last=last->next;
                last->next=NULL;
                last->letter_count=1;
                last->before=malloc(parent->before_length);
                for (int z=x, q=0;z<x+parent->before_length;z++) last->before[q++]=curr->before[z];
                last->after=malloc(1);
                last->after[0]=curr->before[x+parent->before_length-1];
                model->lc_count++;
            } else {
                cur->after=realloc(cur->after,cur->letter_count+1);
                cur->after[cur->letter_count] = curr->before[parent->before_length-1];
                cur->letter_count++;
            }
        }

        curr=curr->next;
    }
    

    model->before_length=parent->before_length;

    return model;
}

int main(int argc, char* argv[]) {
    
    // seed random for possible generation
    srand((unsigned)time(NULL));
    
    for (int i=0;i<argc;i++) {
        if (cmat(argv[i],"-h")||cmat(argv[i],"--help")) {
            return print_help();
        }
        if (cmat(argv[i],"-w")||cmat(argv[i],"--write")) {
            write_file=1;
        }
        if (cmat(argv[i],"-t")||cmat(argv[i],"--train")) {
            do_train=1;
        }
        if (cmat(argv[i],"-i")) {
            input_file=malloc(strlen(argv[i+1])+1);
            sprintf(input_file,"%s",argv[i+1]);
            i++;
            continue;
        }
        if (cmat(argv[i],"-c")) {
            combine_file_child=malloc(strlen(argv[i+1])+1);
            sprintf(combine_file_child,"%s",argv[i+1]);
            i++;
            continue;
        }
        if (cmat(argv[i],"-o")) {
            output_file=malloc(strlen(argv[i+1])+1);
            sprintf(output_file,"%s",argv[i+1]);
            i++;
            continue;
        }
        if (cmat(argv[i],"-l")) {
            print_len=stoi(argv[i+1]);
            i++;
            continue;
        }
    }

    int print_helper=1;

    if (combine_file_child!=NULL&&input_file!=NULL&&output_file!=NULL) {
        // combine smodels
        struct smodel* parent = read_file(input_file);
        struct smodel* child = read_file(combine_file_child);

        struct smodel* combination = combine_models(parent,child,NULL);

        // free models without writing
        write_smodel(parent, SMODEL_FREE);
        write_smodel(child, SMODEL_FREE); 

        print_model(combination);

        // write model
        write_smodel(combination, SMODEL_WRITEFREE);
        print_helper=0;
    }

    if (do_train) {
        if (input_file!=NULL&&output_file!=NULL) {
            if (print_len!=-1) {
                lc_size=print_len;
            }
            struct smodel* model = train();

            if (model==NULL) {
                return 1;
            }

            write_smodel(model, SMODEL_WRITEFREE);
            print_helper=0;
        }
    }

    if (input_file!=NULL&&!do_train&&combine_file_child==NULL) {
        struct smodel* model = read_file(input_file);

        if (model==NULL) {
            printf("Error reading model file\n");
            return 1;
        }
        if (print_len==-1) {
            printf("Target generation length not defined, use -l [length]\n");
            return 1;
        }

        generate(model, print_len, write_file);

        if (!write_file)
            printf("\n");

        write_smodel(model, SMODEL_FREE);

        print_helper=0;
    }

    if (input_file!=NULL)
        free(input_file);
    if (output_file!=NULL)
        free(output_file);
    if (combine_file_child!=NULL)
        free(combine_file_child);

    if (print_helper)
        printf("sigma bot v1\nuse -h or --help\n");

    return 0;
}
