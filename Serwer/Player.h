class Player {
    public:
        Player();
        Player(int n);
        ~Player();
        int getFd();
        void setFd(int n);
        int getPoints();
        void setPoints(int n);
        int getLifes();
        void setLifes(int n);
        bool isReady();
        void setReady(bool b);
        void addPoints(int points);
        void subtractPoints(int points);
        void loseLife();
        int LIFES = 9;
    
    private:
        int fd;
        int points;
        int lifes;
        bool ready;
};
