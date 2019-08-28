

<script type='text/javascript'>
function sliderAdjusted() {
    var slider = document.getElementById("brightnessSlider");
    var output = document.getElementById("currentSliderValue");
    var val = slider.value;
    output.innerHTML = "Current brightness: " + val; // Display the default slider value
    
    var URL = "/api/plugin-apis/Brightness/" + val;
    $.get(URL);
}
function onLoadGetBrightness() {
    var URL = "/api/plugin-apis/Brightness";
    $.get(URL, function (data, status){
          var slider = document.getElementById("brightnessSlider");
          var output = document.getElementById("currentSliderValue");
          slider.value = data;
          output.innerHTML = "Current brightness: " + data;
    });
}
$(document).ready(function() {onLoadGetBrightness()});
</script>
<style>
.slidecontainer {
  width: 100%; /* Width of the outside container */
}

/* The slider itself */
.slider {
  -webkit-appearance: none;  /* Override default CSS styles */
  appearance: none;
  width: 80%; /* Full-width */
  height: 25px; /* Specified height */
  background: #d3d3d3; /* Grey background */
  outline: none; /* Remove outline */
  opacity: 0.7; /* Set transparency (for mouse-over effects on hover) */
  -webkit-transition: .2s; /* 0.2 seconds transition on hover */
  transition: opacity .2s;
}

/* Mouse-over effects */
.slider:hover {
  opacity: 1; /* Fully shown on mouse-over */
}

/* The slider handle (use -webkit- (Chrome, Opera, Safari, Edge) and -moz- (Firefox) to override default look) */
.slider::-webkit-slider-thumb {
  -webkit-appearance: none; /* Override default look */
  appearance: none;
  width: 25px; /* Set a specific slider handle width */
  height: 25px; /* Slider handle height */
  background: #4CAF50; /* Green background */
  cursor: pointer; /* Cursor on hover */
}

.slider::-moz-range-thumb {
  width: 25px; /* Set a specific slider handle width */
  height: 25px; /* Slider handle height */
  background: #4CAF50; /* Green background */
  cursor: pointer; /* Cursor on hover */
}
</style>


<div id="global" class="settings">
<fieldset>
<legend>Brightness Plugin</legend>

<div id="currentSliderValue">Current brightness: 100</div>
<input type="range" min="0" max="200" value="100" class="slider" id="brightnessSlider" oninput="sliderAdjusted()" >


<p>
<p>
The Brightness plugin also provides a REST api and MQTT api for controlling the brightness.
<p>
For REST, use a URL like http://{ip}/api/plugin-apis/Brightness/100 to set the brightness
to 100.
<p>
For MQTT, the sub-topic is "/Brightness" and the payload would be the brightness.
<p>
If  FPP is running in Master mode, changes to the master brightness will also be sent to all the remotes that also have the Brightness plugin installed.
</fieldset>
</div>
