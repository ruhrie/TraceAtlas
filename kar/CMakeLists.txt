#install(FILES DNN.h5 sklearnParams.pkl DESTINATION bin)
install(FILES kar.py DESTINATION bin PERMISSIONS WORLD_EXECUTE)
install(FILES passwd.json DESTINATION bin OPTIONAL)
