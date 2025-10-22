#include <stdio.h>

int gainPalyer1 = 0 ; 
int gainPlayer2 = 0 ; 

void init (int tab [12])
{
    for (int i=0; i<12; i++)
    {
        tab[i]=4 ; 
    }
}

int game (int tab[12] , int playerNumber) 
{
    int gain = 0 ; 
    int pit ; 
    printf("choose your pit from 1 to 6 \n") ;
    scanf ("%d",&pit) ;

     while (pit < 1 || pit > 6 || 
          (playerNumber == 1 && tab[pit - 1] == 0) ||
          (playerNumber == 2 && tab[pit + 5] == 0)) 
    {
        printf("Invalid choice. Choose a pit between 1 and 6 that is not empty:\n");
        scanf("%d", &pit);
    }


    
    if (playerNumber == 2 ) { pit = pit + 6 ; }
    int n = tab[pit-1];
    tab[pit-1] = 0 ; 
    for (int i = 0 ; i < n ; i++)
    {
        if (pit>11) {pit=pit%12;}
        tab[pit]++ ; 
        pit++ ;  
    }
    pit-- ; 
    
    if (tab[pit] == 2 || tab[pit] == 3 ) 
    {
        switch (tab[pit] )
        {
            case 2 : gain+=2 ; tab[pit]=0 ; break;  
            case 3 : gain+=3;  tab[pit]=0 ; break;
        }
       
       for (int i = 0 ; i < n ; i++)
        {
            if (pit<0) {pit=11;}
            switch (tab[pit])
            {
                case 2 : gain+=2 ; tab[pit]=0 ; break;  
                case 3 : gain+=3;  tab[pit]=0 ; break; 
            }  
            
            pit -- ; 

        } 
    }   
    
    return gain ;
}



int main () 
{
    int board[12] ;
    init(board) ; 
    int gainPalyer1 = 0 ; 
    int gainPlayer2 = 0 ; 
    int round = 1 ; 
    while (gainPalyer1 < 25 && gainPlayer2 < 25)
    {
        if (round%2) 
        {gainPalyer1 += game(board,1) ;}
        else {gainPlayer2 += game(board,2) ;}
        round++ ; 
    }   
    return 0 ; 
    

}