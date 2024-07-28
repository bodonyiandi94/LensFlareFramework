
%================================================================%
function starburst(apertureName, noiseName)
    %================================================================%
    % load the images
    apertureTexture = imread(apertureName);
    noiseTexture = imread(noiseName);
    apertureTexture = apertureTexture;%.*noiseTexture;

    imsize = size(apertureTexture);

    %================================================================%
    % compute the Fraunhofer approximation
    apertureFFT = fftshift(fft2(apertureTexture)) / (imsize(1) * imsize(2));
    aperturePow = apertureFFT.*conj(apertureFFT);
    fraunhofer = mat2gray(log(1 + aperturePow));
    
    %================================================================%
    % save the images
    imwrite(fraunhofer, 'apertureFFTClean.bmp');

    % display the image
    imshow(fraunhofer);
end
