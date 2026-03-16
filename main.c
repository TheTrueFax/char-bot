#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

// Simple compare strings function
int cmat(char* a, char* b) {
    if (strlen(a)!=strlen(b)) {
        return 0;
    }
    for (int i=0;i<strlen(a);i++) {
        if (a[i]!=b[i]) {
            return 0;
        }
    }   
    return 1;
}

// Simple power
int apow(int a, int b) {
    if (b==0) {
        return 1;
    }
    int result = a;
    for (int i=0;i<b-1;i++) {
        result*=a;
    }
    return result;
}

// String to int
int stoi(char* str) {
    int result = 0;
    int is_negative = 0;
    int alen = strlen(str);
    if (str[0]=='-') {
        is_negative=1;
    }
    if (alen==is_negative+1) {
        return str[is_negative]-'0';
    }
    int index = is_negative;
    for (int i=apow(10,alen-is_negative-1);index<alen;i/=10) {
        if (is_negative) {
            result-=(str[index]-'0')*i;
        } else {
            result+=(str[index]-'0')*i;
        }
        index++;
    }
    return result;
}

#define SMODEL_VERSION 1

struct lcollection {
    uint32_t letter_count;
    char* before;
    char* after;
    int set;

    struct lcollection* next;
};

struct smodel {
    uint8_t version;

    uint16_t name_length;
    char* name;

    uint8_t before_length;

    /*uint32_t file_len;
    char* sfile;*/

    uint32_t lc_count;
    struct lcollection lc_first;
};

int print_help() {
    printf("bot help\n\
\n\
-i [file]   |  specify input file\n\
-o [file]   |  creates or writes to a file for output\n\
-l  [i]     |  set target length for output, or change target training memory size\n\
-t, --train |  changes the mode to training mode, where it will create a model\n\
-w, --write |  do not print generated text, write to the output file instead\n\
-h, --help  |  show this menu\n");
}

char* input_file = NULL;
char* output_file = NULL;

int print_len = -1;
int write_file = 0;

int do_train = 0;

int lc_size = 2;

// Write model to file and free memory
void write_smodel(struct smodel* model) {
    if (output_file==NULL){
        printf("Tried to write with no output file?\n");
        return;
    }
    
    FILE* fp;

    fp = fopen(output_file,"wb");
    if (fp==NULL) {
        printf("Failed to open file for writing?\n");
        return;
    }

    fwrite(&model->version,1,1,fp); // version

    uint32_t thingyi = strlen(model->name)+1;
    
    fwrite(&thingyi,2,1,fp); // name length
    fwrite(model->name,1,strlen(model->name)+1,fp); // name
    
    free(model->name);

    
    fwrite(&model->before_length,1,1,fp); // before length
    fwrite(&model->lc_count,4,1,fp); // lc count

    struct lcollection* curr = &model->lc_first; // selected lcollection

    struct lcollection* tofree; // lcollection to free
    int onfirst=1;
    while (1) {
        fwrite(&curr->letter_count,4,1,fp);

        for (int i=0;i<model->before_length;i++) {
            fwrite(&curr->before[i],1,1,fp);
        }
        
        fwrite(curr->after,1,curr->letter_count,fp);

        free(curr->before);
        free(curr->after);
        
        if (curr->next==NULL) {
            free(curr);
            return;
        }
        tofree=curr;
        curr=curr->next;
        if (!onfirst)
            free(tofree);
        onfirst=0;
    }

    fclose(fp);
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
    snprintf(model->name,(strlen(input_file)+1),"%s",input_file);

    model->before_length=lc_size;
    model->lc_count=0;

    char* lc = malloc((size_t)lc_size);
    char curr;

    uint32_t index=0;

    uint32_t size;

    struct lcollection* lc_last=&model->lc_first;

    if (fseek(fp, 0, SEEK_END) == 0) {
        size = ftell(fp);
    }
    fseek(fp, 0, SEEK_SET);

    while (index<size) {
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
                //printf("%c%c %c%c\n",lc[0],lc[1],cur->before[0],cur->before[1]);

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
                //printf("Duplicate: %c%c\n",lc[0],lc[1]);
                char* allocd = malloc(sizeof(char)*(cur->letter_count+1));
                for (int i=0;i<cur->letter_count;i++) {
                    allocd[i] = cur->after[i];
                }
                allocd[cur->letter_count] = curr;
                cur->letter_count++;
                free(cur->after);
                cur->after=allocd;
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

    free(lc);
    fclose(fp);

    return model;
}

#define BLOCK_SIZE 500

void write_char(char ch, FILE* fp) {
    if (fp==NULL) {
        printf("%c",ch);
    } else {
        fwrite(&ch,1,1,fp);
    }
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

    // populate lc with first values
    for (int i=0;i<model->before_length;i++) {
        lc[i]=model->lc_first.before[i];
        write_char(lc[i],fp);
    }

    for (int i=model->before_length;i<target;i++) {
        // Find matching LC
        struct lcollection* curr = &model->lc_first;
        int wasfound=0;
        for (int x=0;x<model->lc_count;x++) {
            if (cmat(lc,curr->before)) {
                wasfound=1;
                break;
            }
            if (curr->next==NULL) {
                break;
            }
            curr=curr->next;
        }

        if (!wasfound) { // No letter found to come next, presume end of file.
            break;
        }

        // Randomly pick letter
        char chosen = curr->after[rand()%curr->letter_count];
        write_char(chosen,fp);

        for (int x=0;x<model->before_length;x++) {
            if (x==model->before_length-1) {
                lc[x]=chosen;
            break;
            }
            lc[x]=lc[x+1];
        }
    }

    if (fp!=NULL) {
        fclose(fp);
    }
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

    return model;
    fclose(fp);
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

    if (do_train) {
        if (input_file!=NULL&output_file!=NULL) {
            if (print_len!=-1) {
                lc_size=print_len;
            }
            struct smodel* model = train();

            if (model==NULL) {
                return 1;
            }

            write_smodel(model);

            return 0;
        }
    }

    if (input_file!=NULL&&!do_train) {
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

        printf("\n");
        //print_model(model);
        
        return 0;
    }

    if (print_len!=-1)
        printf("%i\n",print_len);

    if (input_file!=NULL)
        free(input_file);
    if (output_file!=NULL)
        free(output_file);

    printf("sigma bot v1\nuse -h or --help\n");

    return 0;
}
