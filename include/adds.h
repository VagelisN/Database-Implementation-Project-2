#ifndef ADDS_H
#define ADDS_H

#define AM_MAX_OPEN_FILES 20
#define AM_MAX_OPEN_SCANS 20

#include "AM.h"
#include "bf.h"

typedef struct Stack_node
{
	int data;
	struct Stack_node* next;
}stack_node;

/* Κλασικές συναρτήσεις στοίβας. Χρησιμοποιούνται για την διατήρηση μονοπατιού από index blocks*/
int push(stack_node** ,int);
int pop(stack_node**);
void delete_stack(stack_node*);

/* Δημηουργεί και αρχικοποιεί ένα block. Αν στο b_r δοθεί 0 το block αρχικοποιείται
 * ως index block αλλιώς αρχικοποιείται ως data block
 * Ο πρώτος Int και στις δυο περιπτώσεις είναι ο τύπος του block(data 1/index 0)
 * Ο δεύτερος Int έχει πόσες εγγραφές/κλειδιά έχει το block
 * O τρίτος Ιnt στο data block είναι ο δείκτης στο επόμενο ενώ στο index block ο πρώτος δείκτης 
 * αριστερά απο το πρώτο κλειδί*/
 
int create_block(BF_Block **,int,open_file_info_s*);

/* Τρόπος υπολογισμού μεγιστου αριθμού κλειδιών ( n ) που χωράνε σε ενα index block:
 * BLOCK_SIZE >= (n+1)*sizeof(int)+n*attrLenght1 + 2*sizeof(int)
 * οπου ο τελευταίος παράγοντας που προστείθεται είναι οι πληροφορίες που χρειαζόμαστε σε κάθε block
 * ------> n <=( BLOCK_SIZE - 3*sizeof(int) )/ (attrLength1+sizeof(int))
 *
 * Για τον αριθμό των εγγραφών ( n ) που χωράνε σε ένα data block:
 * BLOCK_SIZE >= n*(attrLength1+attrLenght2)+3*sizeof(int)
 * Οπου πάλι ο τελευταίος παράγοντας είναι οι πληροφόρίες
 * ------> n <=( BLOCK_SIZE - 3*sizeof(int) )/ (attrLength1+attrLenght2) */

void compute_max_entries(open_file_info_s*);

/* Συγκρίνει τις τιμές key1 , key2. Επιστρέφει 0 για ίσον
 * αρνητικό για key1 < key2 και θετικό για key1 > key2*/
 
int key_compare(void*, void*, char);

/* Βρίσκει σε ποιό block είναι/μπορεί να εισαχθεί το κλειδί key και κρατάει
 * το μονοπάτι απο index block που ακολούθησε για να το βρει στην stack*/
 
int find_block(void* , stack_node**,open_file_info_s*);


/* Παίρνει τα δεδομένα ενός block μαζί με μια ενα κλειδί αν κληθεί για index block
 * Ή ένα ζευγάρι τιμής κλειδιού αν κληθεί για data block. Ταξινομεί στη συνέχεια το
 * block τοποθετώντας στην σωστή θέση το κλειδί/κλειδί-τιμή.Το gen ειναι συντομογραφία 
 * του general καθώς έχει φτιαχτεί να λειτουργέι και για τους δυο τύπους block*/
 
void sort_block(char* ,void* ,void* ,open_file_info_s*);


/* Βρίσκει σε ποιά θέση μέσα σε ένα block πρέπει να εισαχθεί μια εγγραφή
 * Και αυτή δουλεύει ανεξάρτητα αν το block είναι index η data */

int find_place_in_block(char* ,void* ,open_file_info_s*);


/* Υπολογίζει πόσα ίδια κλειδιά υπάρχουν στο σημείου που πρόκειται να σπάσει ένα block και επιστρέφει
 * το νέο σημείο ώστε όλα τα ίδια κλειδιά να μένουν στο ίδιο block*/

int calculate_same_keys(char*,int, open_file_info_s*);


/* Σπάει ένα block στα δυο δημηουργόντας ένα καινούριο και μοιράζοντας τις εγγραφές.
 * στο ktgu(key to go up) αποθηκεύεται η τιμή από το δεξί block του κλειφιού που πρέπει να ανέβει στο επάνω
 * επίπεδο από index blocks*/

int break_block(char*, void*, void*, void**, open_file_info_s*);


/*Απλώς παίρνει το πρώτο data block κατεβαίνοντας τους αριστερούς δείκτες των index blocks*/

int first_dblock(open_file_info_s*);


/* Ελέγχει αν ισχύει η συνθήκη του op */

int os_compare(void *,void *,int,char);

/*Εκτύπωση data(red) και index(black) block. Δεν χρησιμοποιούνται κάπου, είναι για debugging*/
void print_red_block(char* ,open_file_info_s* );
void print_black_block(char*,open_file_info_s*);

#endif
