clear
path = './fir-input-kernel/';
inputWorking = importdata( fullfile (path,'inputWorkingSet.txt'));
outputWorking = importdata(fullfile (path,'outputWorkingSet.txt'));
WorkingSet = importdata(fullfile (path,'WorkingSet.txt'));
Datasize1  = size(inputWorking,1);
figure_x1 = linspace(0,Datasize1-1,Datasize1);
Datasize2  = size(outputWorking,1);
figure_x2 = linspace(0,Datasize2-1,Datasize2);
Datasize3  = size(WorkingSet,1);
figure_x3 = linspace(0,Datasize3-1,Datasize3);

figure(1)
plot(figure_x1, inputWorking)
box off
xlabel('Function of time');
ylabel('Living addresses number');
title('Input Working Set');
figure(2)
plot(figure_x3, WorkingSet)
box off
xlabel('Function of time');
ylabel('Living addresses number');
title('Internal Working Set');
figure(3)
plot(figure_x2, outputWorking)
box off
xlabel('Function of time');
ylabel('Living addresses number');
title('Output Working Set');

