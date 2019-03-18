#ifndef AM_H_
#define AM_H_

/* Error codes */

extern int AM_errno;
 
#define AME_OK 0
#define AME_EOF -1
#define AME_ERROR -2
#define AME_FILE_NOT_FOUND -3
#define AME_NO_FILE_SPACE -4
#define AME_WRONG_FILE_TYPE -5
#define AME_BF_ERROR -6
#define AME_SCAN_NOT_FOUND -7
#define AME_TYPE_LENGTH_MISMATCH -8
#define AME_FILE_IS_OPEN -9
#define AME_INSERT_ERROR -10
#define AME_FILE_ALREADY_EXISTS -11
#define AME_OPEN_SCAN_FOR_THIS_FILE -12
#define AME_NO_SCAN_SPACE -13
#define AME_BLOCK_NOT_FOUND -14
 
#define EQUAL 1
#define NOT_EQUAL 2
#define LESS_THAN 3
#define GREATER_THAN 4
#define LESS_THAN_OR_EQUAL 5
#define GREATER_THAN_OR_EQUAL 6
 
 
typedef struct am_attr_types
{
    char attrType1;
    int attrLength1;
    char attrType2;
    int attrLength2;
}am_attr_types_s;
 
typedef struct open_file_info
{
  int fd;
  int b_root;
  char* filename;
  int max_entries,max_keys;
  am_attr_types_s attributes;
}open_file_info_s;
 
 
typedef struct open_scan_info
{
  int fd;
  char* filename;
  int start_block;
  int key_block;
  int end_block;
  int next_record;
  int op;
  void *value;
}open_scan_info_s;

/* Αρχικοποιεί το επίπεδο BF. Στη συνέχεια αρχικοποιεί τους δυο πίνακες με τα ανοιχτά αρχεία
 * και τα ανοιχτά scan με AM_MAX_OPEN_FILES και AM_MAX_OPEN_SCANS θέσεις αντοίστοιχα.
 * Κάθε θέση του πίνακα είναι δείκτης σε μια δομή η οποία περιέχει τις
 * απαραίτητες πληροφορίες για τη διαχείρηση του ανοιχτού αρχείου/scan.*/

void AM_Init( void );


/* Κοιτάζει αν το αρχείο που ζητήθηκε να δημιουργηθεί υπάρχει ήδη και γυρίζει
 * κωδικό λάθους αν υπάρχει. Στη συνέχεια κοιτάζει αν τα ζευγάρια Τύπου Μεγέθους
 * που δόθηκαν είναι αποδεκτά και γυρίζει κωδικό λάθους στην περίπτωση που δεν είναι.
 * Τέλος δημηουργεί το αρχείο και γράφει στο πρώτο του μπλοκ τα εξής: 1)τον αριθμό 206
 * ως αναγνωριστικό για AM file. 2)Τα attributes που δόθηκαν 3) Τον αριθμό -1 που
 * δείχνει οτι δεν υπάρχει ρίζα ακόμα.*/

int AM_CreateIndex(
  char *fileName, /* όνομα αρχείου */
  char attrType1, /* τύπος πρώτου πεδίου: 'c' (συμβολοσειρά), 'i' (ακέραιος), 'f' (πραγματικός) */
  int attrLength1, /* μήκος πρώτου πεδίου: 4 γιά 'i' ή 'f', 1-255 γιά 'c' */
  char attrType2, /* τύπος πρώτου πεδίου: 'c' (συμβολοσειρά), 'i' (ακέραιος), 'f' (πραγματικός) */
  int attrLength2 /* μήκος δεύτερου πεδίου: 4 γιά 'i' ή 'f', 1-255 γιά 'c' */
);


/* Διαγράφει το αρχείο εφόσον αυτό δεν είναι ανοιχτό σε κανέναν από τους δυο πίνακες
 * open_scans, open_files.*/
 
int AM_DestroyIndex(
  char *fileName /* όνομα αρχείου */
);


/* Ανοίγει ενα αρχείο και αποθηκεύει τον file descriptor του μαζί με άλλες πληροφορίες
 * στον πίνακα open_files αν αυτός δεν είναι γεμάτος και αν το αρχείο που ζητήθηκε να άνοιχτεί
 * είναι AM αρχείο. Σε περίπτωση επιτυχίας γυρίζει την θέση του πίνακα στην οποία βρίσκονται
 * οι πληροφορίες για το ανοιχτό αρχείο αλλιώς γυρίζει κωδικό λάθους*/
 
int AM_OpenIndex (
  char *fileName /* όνομα αρχείου */
);


/* Κλείνει το αρχείο που βρίσκεται στην θέση fileDesc του πίνακα open_files, αν αυτό
 * δεν είναι ανοιχτό για scan στον πίνακα open_scans. Αν είναι ανοιχτό στον open_scans
 * γυρίζει κωδικό λάθους.*/
 
int AM_CloseIndex (
  int fileDesc /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
);



/* Εισάγει την εγγραφή με κλειδί: value1 και τιμή value2 στο αρχείο που βρίσκεται
 * στην θέση fileDesc του open_files.
 * 
 * Οι περιπτώσεις εισαγωγής χωρίζονται στις παρακάτω:
 * 1)Αν το αρχείο ΔΕΝ έχει ρίζα και ΔΕΝ έχει εγγραφές, τότε δημηουργείται ΜΟΝΟ ενα block δεδομένων και εισάγεται η εγγραφή
 * 
 * 2)Αν το αρχείο ΔΕΝ έχει ρίζα αλλά έχει εγγραφές τότε σίγουρα αυτές είναι στο block 1 (αφου το 0 έχει τα metadata).
 * 	 Αν λοιπόν το μπλοκ 1 έχει χώρο απλώς εισάγεται η εγγραφή εκεί, αλλιώς σπάει σε δυο, μοιράζονται οι εγγραφές,και
 *   δημηουργείται ρίζα που δείχνει σε αυτά τα δυο μπλοκ
 * 
 * 3)Αν το αρχείο έχει ρίζα βρίσκει σε ποιό μπλοκ δεδομένων θα έμπαινε η εγγραφή, ακολουθόντας τα index blocks και κρατόντας
 * 	 το μονοπάτι που ακολούθησε σε μια στοίβα.
 * 		-Αν υπάρχει χώρος στο block δεδομένων τότε η εγγραφή απλώς εισάγεται εκεί
 *      -Αλλιώς σπάει το block δεδομένων σε δυο μοιράζονται οι εγγραφές και το αριστερότερο κλειδί του νεου block
 *       πρέπει να εισαχθεί στα index blocks. Ξεκινάει και αδειάζει τη στοίβα που περιέχει το μονοπάτι που ακολούθησε 
 *       για να βρει το block δεδομένων:
 * 			-Αν το index block έχει χώρο απλώς εισάγει το κλειδί εκεί
 * 			-Αλλιώς σπάει το index block σε δυο μοιράζει τα κλειδιά, και ανεβάζει το αριστερότερο κλειδί του νέου block
 *     		 στον "πατέρα" που υποδεικνύεται από την στοίβα.
 * 			-Αν έφτασε στη ρίζα σπάει τη ρίζα και δημηουργεί νέα ρίζα */

int AM_InsertEntry(
  int fileDesc, /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
  void *value1, /* τιμή του πεδίου-κλειδιού προς εισαγωγή */
  void *value2 /* τιμή του δεύτερου πεδίου της εγγραφής προς εισαγωγή */
);


/* Η AM_OpenIndexScan φτιαχνει θεση στον πίνακα open_scans και την αρχικοποιεί με της απαραίτητες πληροφορίες.
 * επισης καθοριζει απο ποιο μπλοκ(start block) να ξεκινησει να ψαχνει η findnext και σε ποιο να τελειωσει(end block)
 * αναλογα το operator που δόθηκε.
 * Το keyblock ειναι το μπλοκ που θα βρισκοταν/βρίσκεται το value αν γινοταν insert. Στο μπλοκ αυτό ελεγχεται το operator
 * καθώς σε όλα τα υπόλοιπα δεν χρειάζεται κάτι τέτοιο */

int AM_OpenIndexScan(
  int fileDesc, /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
  int op, /* τελεστής σύγκρισης */
  void *value /* τιμή του πεδίου-κλειδιού προς σύγκριση */
);

/* Η AM_FindNextEntry στελνει καθε entry απο το start block
 * μεχρι το end block και ελεγχει για το Operator μονο μεσα στο keyblock 
 * αυτο μας το επιτρεπουν τα εξής:
 * 	1)Τα entries ειναι ταξινομημενα στα blocks
 * 	2)Η υπόθεση οτι όμοια entrys χωρανε σε ενα μπλοκ(εκφωνηση) */
 
void *AM_FindNextEntry(
  int scanDesc /* αριθμός που αντιστοιχεί στην ανοιχτή σάρωση */
);


/* Κλείνει το αρχείο που βρίσκεται στην θέση scanDesc του πίνακα open_scans, αν αυτό
 * είναι ανοιχτό για scan στον πίνακα open_scans. Αν δεν υπάρχει ανοιχτό scan στην θέση
 * scanDesc του open_scans γυρίζει κωδικό λάθους.*/
 
int AM_CloseIndexScan(
  int scanDesc /* αριθμός που αντιστοιχεί στην ανοιχτή σάρωση */
);

/* Τυπώνει το κατάλληλο μήνυμα ανάλογα με την τιμή της globsl μεταβλητής AM_errno*/

void AM_PrintError(
  char *errString /* κείμενο για εκτύπωση */
);

/* Διαγράφει τους δυο πίνακες με ανοιχτά αρχεία και ανοιχτά scans και κλείνει το επίπεδο BF*/

void AM_Close();


#endif /* AM_H_ */
