namespace maxwell {

class Probes {
    public:
        int vis_steps = 1;
        int precision = 8;
        bool paraview = false;
        bool glvis = false;
        bool extractDataAtPoint = false;
        FieldType fieldToExtract = FieldType::E;
        IntegrationPoint integPoint;
    private:
};

}