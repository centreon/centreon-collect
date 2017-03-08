# backbone-relational-jsonapi

Overloads `Backbone.Collection` and `Backbone.RelationalModel` `parse` methods to add compatibility with the [JSONapi](http://jsonapi.org/) protocol.

## Installation

    $ bower install backbone-relational-jsonapi

## Documentation

Backbone and Backbone.Relational and Underscore need to be loaded before this library. Then, the `parse` methods are overloaded. For example, using RequireJS, you'll need to shim it as follows

    require.config({
        paths : {
            'backbone': 'path/to/backbone',
            'backbone-relational': 'path/to/backbone-relational',
            'backbone-relational-jsonapi': 'path/to/backbone-relational-jsonapi'
        }
        shim: {
            'backbone' : {
                exports : 'Backbone',
                deps : ['jquery','underscore']
            },
            'backbone-relational': {
                deps: ['backbone']
            },
            'backbone-jsonapi' : {
                deps : ['backbone', 'backbone-relational','underscore']
            },
        }

To use it, you can require it at your application's boostrap like

    define([
        "backbone",
        "backbone-relational",
        "backbone-jsonapi"
    ], function(Backbone) {
        // Your application here
    });

And your instance of `Backbone` will use the library.

## Currently supported

The library is currently able to parse

* The `data` object
* The `relationships` objects
* The `included` objects

### Parsing a compound object

To parse a compound object, the library first checks if something is present in the `included` object and creates the corresponding instances using the `id` and `type` attributes. To do this, it uses a common model factory that looks up the class names depending on the `type`.

In your classes, specify a default `type` value, like

    var Tag = Backbone.RelationalModel.extend({
        defaults: {
            type: 'tags'
        }
    }

and then register this class in the model factory, like

    Backbone.modelFactory.registerModel(Tag);

This way, compound objecs containing `tags` objects will be parsed and the included instances of `Tag` will be available to other objects for relationships.

## Meta objects

Meta objects are supported and their treatment is delegated to the model or collection that is parsing the incoming data. When a `meta` object is found within the response, the function `handleMeta` is called on `this`. If the function is not defined, then the `meta` object is ignored.
Be careful: this function is called before the `parse` function has actually returned, so you won't be able to access the parsed data from the `handeMeta` scope.

## Examples

Here's an example of an `articles` object that can be parsed by the library

    {
        "data": {
            "type": "articles",
            "id": "1",
            "attributes": {
                "title": "The title of the article",
                "url": "http://article.com/article-id",
                "date": 1423094400,
                "thumbnail": "thumbnail.png",
            },
        }
        "relationships": {
            "tags": {
                "data": [
                    {
                        "type": "tags",
                        "id": "10"
                    },
                ]
            }
        },
        "included": [
            {
                "type": "tags",
                "id": "10",
                "attributes": {
                  "name": "geeks"
                }
            }
        ]
    }

Which, Backbone-side would be expressed like

    var Article = Backbone.RelationalModel.extend({
        defaults: {
            type: 'articles'
        },
        relations: [{
            type: Backbone.HasMany,
            key: 'tags',
            relatedModel: Tag // Refers to the Tag class defined above
        }]
    });

## Not supported

Currently, the support of the JSONapi specification is partial. Work still needs to be done.

* Links
