var instructions = require('./instructions.js');
var locals = require('./locals.js');
var menu = require('./menu.js');

module.exports = (function() {

    var markers = [];
    var temporaryMarkers = [];
    var pinIndex = 0;
    var redColour = "F7574C";
    var greenColour = "7BCC70";
    var letterColour = "000000";
    var infoWindow = new google.maps.InfoWindow();

    var add = function(map, position, letter) {
        var marker = createMarker(map, position, letter);
        if (pinIndex > 0) {
            addInstructionToMarker(marker, locals.defaultMarker,
                                   locals.mapText);
        }
        pinIndex++;
        markers.push([marker]);
        return marker;
    };

    var addTemporaries = function(map, results) {
        results.forEach(function(place) {
            var marker = createMarker(map, place.geometry.location,
                                    null, greenColour);

            google.maps.event.addListener(marker, 'click', function() {
                displayInfoBox(map, place, marker);
            });

            addInstructionToMarker(marker, locals.temporaryMarker,
                                 locals.mapText);
            temporaryMarkers.push(marker);
        });
    };

    var convertTemporariesToPermanent = function() {

        var bulk = [];
        for (var i = 0; i < temporaryMarkers.length; i++) {
            var temporaryMarker = temporaryMarkers[i];
            var marker = createMarker(temporaryMarker.map, temporaryMarker.position);
            bulk.push(marker);
        }
        clearTemporaries();

        pinIndex++;
        markers.push(bulk);
    };

    var setOrigin = function(map, position) {
        if (markers.length === 0) {
            add(map, position, 'A');
        } else {
            var previous = markers[0][0];
            markers[0][0] = createMarker(map, position, 'A');
            previous.setMap(null);
        }
        markers[0][0].isOrigin = true;
        addInstructionToMarker(markers[0][0], locals.originMarker,
                               locals.mapText);
    };

    var createMarker = function(map, position, letter, colour) {
        colour = colour || redColour;
        letter = letter || String.fromCharCode('A'.charCodeAt(0) + pinIndex);
        var draggable = letter === 'A';
        var myPin = makeIcon(letter, colour);
        var marker = new google.maps.Marker({
            position: position,
            map: map,
            icon: myPin,
            index: pinIndex,
            draggable: draggable
        });
        return marker;
    };

    var makeIcon = function(letter, colour) {
        return new google.maps.MarkerImage("http://chart.apis.google.com" +
               "/chart?chst=d_map_pin_letter&chld=" + letter + "|" +
               colour + "|" + letterColour);
    };

    var displayInfoBox = function(map, place, marker) {

        var content = $('<div>');

        ["Name: " + place.name,
        "Rating: " + (place.rating || "N/A")
        ].forEach(function(line) {
            content.append($("<div>", {
                class: "nowrap"
            }).html(line));
        });

        var button = $('<input>', {
            type: 'button',
            value: 'Add me'
        });
        content.append(button);
        google.maps.event.addDomListener(button[0], 'click', function() {
            add(map, place.geometry.location);
            clearTemporaries();
            menu.disableNextErrandInput();
        });

        infoWindow.open(map, marker);
        infoWindow.setContent(content[0]);
    };

    var remove = function(marker) {
        marker.setMap(null);
        markers.splice(marker.index, 1);
        pinIndex--;
        if (marker.index < pinIndex) {
            reorderMarkers();
        }
    };

    var reorderMarkers = function() {
        for (var i = 0; i < markers.length; i++) {
            var letter = String.fromCharCode('A'.charCodeAt(0) + i);
            var icon = makeIcon(letter, redColour);
            markers[i][0].setIcon(icon);
            markers[i][0].index = i;
        }
    };

    var addInstructionToMarker = function(marker, mouseoverText,
                                          mouseoutText) {
        google.maps.event.addListener(marker, 'mouseover', function() {
            instructions.setText(mouseoverText);
        });

        // When the mouse is away from the marker sets back the map message
        google.maps.event.addListener(marker, 'mouseout', function() {
            instructions.setText(mouseoutText);
        });
    };

    var clear = function() {
        markers.map(function(m) {
            m.map(function(m) {
                m.setMap(null);
            });
        });
        markers = [];
        pinIndex = 0;
    };

    var clearTemporaries = function() {
        temporaryMarkers.map(function(m) { m.setMap(null); });
    };

    return {
        add: add,
        addTemporaries: addTemporaries,
        remove: remove,
        clear: clear,
        clearTemporaries: clearTemporaries,
        getSortedMarkers: function() { return Object.keys(markers).sort(); },
        getOrigin: function() { return markers[0][0]; },
        getMarkers: function() { return markers; },
        setOrigin: setOrigin,
        convertTemporariesToPermanent: convertTemporariesToPermanent
    };

})();
